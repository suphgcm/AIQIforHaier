// AIQIforHaier.cpp : 定义应用程序的入口点。

#define _CRT_SECURE_NO_WARNINGS
#define NOMINMAX
#include "framework.h"
#include "AIQIforHaier.h"
#include "framework.h"
#include <cstdio>
#include <WinSock2.h>
#include "nlohmann/json.hpp"
#include <iostream>
#include <fstream>
#include <codecvt>
#include <string>
#include <locale>
#include <unordered_map>
#include <filesystem>
#include <Windows.h>
#include <cstdlib>
#include <list>
#include <WinHttp.h>
#include <sstream>
#include <thread>
#include <chrono>
#include <limits>

#include "MessageQueue.h"
#include "WZSerialPort.h"
#include "Log.h"
#include "httplib.h"

#pragma comment(lib, "ws2_32.lib")

#define MAX_LOADSTRING 100
#define HTTP_POST_PORT 9003

std::mutex mtx[8];
bool isPinTriggered[8]; // GPIO 针脚是否触发状态
HANDLE hMainWork[8]; // 当前线程句柄
std::list<HANDLE> allMainWorkHandles; // 所有线程句柄
int remoteCtrlPin;

std::string pipelineCode = "CX202309141454000002"; // 一台工控机只跑一个 pipeline
std::string serverIp = "192.168.0.189";
int serverPort = 9003;
int stopFlagPin = 1;
std::string pipelineName;

HINSTANCE hInst;                                // 当前实例
WCHAR szTitle[MAX_LOADSTRING];                  // 标题栏文本
WCHAR szWindowClass[MAX_LOADSTRING];            // 主窗口类名

HWND hEdit;                                     // 编辑栏，用于输出日志

// 此代码模块中包含的函数的前向声明:
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);

// 线程函数
DWORD __stdcall MainWorkThread(LPVOID lpParam);
DWORD __stdcall UnitWorkThread(LPVOID lpParam);

struct counter {
	std::mutex mutex;
	long long count;
} Counter;

std::mutex map_mutex;

std::vector<char> readPCMFile(const std::string& filename) {
    // 打开文件
    std::ifstream file(filename, std::ios::binary | std::ios::ate);

    // 获取文件大小
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    // 创建一个向量来保存文件内容
    std::vector<char> buffer(size);

    // 读取文件
    if (file.read(buffer.data(), size)) {
        return buffer;
    } else {
        // 处理错误
        throw std::runtime_error("Error reading file");
    }
}

void AddTextPart(std::vector<char> &body, std::string &text, std::string &boundary, std::string name)
{
	std::string textPartStart = "Content-Disposition: form-data; name=\""+ name + \
		"\"\r\nContent-Type:text/plain\r\n\r\n";
	body.insert(body.end(), textPartStart.begin(), textPartStart.end());

	std::string textData = text;
	body.insert(body.end(), textData.begin(), textData.end());

	std::string textPartEnd = "\r\n--" + boundary + "\r\n";
	body.insert(body.end(), textPartEnd.begin(), textPartEnd.end());

	return;
}

void AddBinaryPart(std::vector<char>& body, unsigned char* buffer, unsigned int lengh, std::string& boundary, std::string fileName, MSG_TYPE_E binaryType)
{
	std::string contentType;
	if (binaryType == MSG_TYPE_PICTURE) {
		contentType = "image/jpeg";
	}
	else {
		contentType = "audio/basic";
	}

	// Add picture
	std::string partStart = "Content-Disposition: form-data; name=\"files\"; filename=\"" + \
		fileName + "\"\r\nContent-Type: " +  contentType + "\r\n\r\n";
	body.insert(body.end(), partStart.begin(), partStart.end());

	for (int i = 0; i < lengh; i++)
	{
		body.push_back(buffer[i]);
	}

	std::string partEnd = "\r\n--" + boundary + "\r\n";
	body.insert(body.end(), partEnd.begin(), partEnd.end());

	return;
}

void HttpPost(struct httpMsg& msg) {
	httplib::Client cli("192.168.0.189", HTTP_POST_PORT);

	std::string path;
	httplib::Headers headers;
	std::string body;
	std::string contentType;

	if (msg.type == MSG_TYPE_STOP) {
		path = "/api/client/inspection/stopFlag";
		//headers = { {"Content-Type", "application/json"} };
		contentType = "application/json";
		nlohmann::json jsonObject;
		jsonObject["pipelineCode"] = msg.pipelineCode;
		jsonObject["productSn"] = msg.productSn;
		body = jsonObject.dump();
	}
	else {
		path = "/api/client/inspection/upload";
		//headers = { {"Content-Type", "multipart/form-data;boundary=----WebKitFormBoundary7MA4YWxkTrZu0gW"} };
		contentType = "multipart/form-data;boundary=----WebKitFormBoundary7MA4YWxkTrZu0gW";
		std::string boundary = "----WebKitFormBoundary7MA4YWxkTrZu0gW";
		std::vector<char> body1;

		//add first boundary
		std::string partStart = "--" + boundary + "\r\n";
		body1.insert(body1.end(), partStart.begin(), partStart.end());

		if (msg.type == MSG_TYPE_PICTURE) {
			for (auto it = msg.pictures.begin(); it != msg.pictures.end(); ++it)
			{
				std::string imageName = std::to_string(it->sampleTime) + ".jpeg";
				// Add picture
				AddBinaryPart(body1, it->imageBuffer, it->imageLen, boundary, imageName, MSG_TYPE_PICTURE);
			}
		}
		else if (msg.type == MSG_TYPE_TEXT) {
			// Add TEXT
			AddTextPart(body1, msg.text, boundary, "content");
		}
		else if (msg.type == MSG_TYPE_SOUND) {
			std::string soundPath = projDir.c_str();
			soundPath.append("\\temp\\");
			std::string soundName = std::to_string(msg.sampleTime) + ".pcm";
			if (!std::filesystem::exists(soundPath + soundName))
			{
				log_error("Target pcm file does not exist! fileName: " + soundName);
				return;
			}
			auto sound = readPCMFile(soundPath + soundName);
			// Add sound
			AddBinaryPart(body1, (unsigned char*)sound.data(), sound.size(), boundary, soundName, MSG_TYPE_PICTURE);
		}
		else {
			log_error("Invalid msg type, msgType: " + std::to_string(msg.type) + ", msgId: " + std::to_string(msg.msgId));
		}

		std::string sampleTime = "123456789";
		// Add text part
		AddTextPart(body1, msg.pipelineCode, boundary, "pipelineCode");
		AddTextPart(body1, msg.processesCode, boundary, "processesCode");
		AddTextPart(body1, msg.processesTemplateCode, boundary, "processesTemplateCode");
		AddTextPart(body1, msg.productSn, boundary, "productSn");
		AddTextPart(body1, msg.productSnCode, boundary, "productSnCode");
		AddTextPart(body1, msg.productSnModel, boundary, "productSnModel");
		AddTextPart(body1, sampleTime, boundary, "sampleTime");

		body1.pop_back();
		body1.pop_back();
		std::string End = "--\r\n";
		body1.insert(body1.end(), End.begin(), End.end());

		std::string temp(body1.begin(), body1.end());
		body = temp;
	}

	log_info("Start post http msg, msgId: " + std::to_string(msg.msgId));
	auto res = cli.Post(path.c_str(), headers, body, contentType);
	if (res) {
		if (res->status == 200) {
			log_info("Http msg post successed! msgId: " + std::to_string(msg.msgId));
		}
		else {
			log_error("Http msg post failed! resp code:" + std::to_string(res->status));
		}
	}
	else {
		auto err = res.error();
		log_error("Http msg post failed! http err:" + httplib::to_string(err));
	}
	log_info("End post http msg, msgId: " + std::to_string(msg.msgId));

	return;
}

DWORD HttpPostThread(LPVOID lpParam)
{
	struct httpMsg msg;

	while (true)
	{
		Singleton::instance().wait(msg);
		if (msg.type == MSG_TYPE_STOP)
		{
			log_info("Process http stop msg, msgId: " + std::to_string(msg.msgId) + \
				", processSn: " + msg.productSn);
		}
		else {
			log_info("Process http msg, msgId: " + std::to_string(msg.msgId) + ", processSn: " + msg.productSn + \
				", processesTemplateCode : " + msg.processesTemplateCode);
		}

		HttpPost(msg);

		if (msg.type == MSG_TYPE_STOP)
		{
			log_info("End process http stop msg, msgId: " + std::to_string(msg.msgId) + \
				", processSn: " + msg.productSn);
		}
		else {
			log_info("End process http msg, msgId: " + std::to_string(msg.msgId) + \
				", processSn: " + msg.productSn + ", processesTemplateCode : " + msg.processesTemplateCode);
		}

		if (msg.type == MSG_TYPE_PICTURE)
		{
			for (auto it = msg.pictures.begin(); it != msg.pictures.end(); ++it)
			{
				delete[] it->imageBuffer;
			}
		}
	}

	return 0;
}

class MessageQueue<struct gpioMsg> GpioMessageQueue;

void GpioMsgProc(struct gpioMsg& msg)
{
	int message = msg.message;
	UINT gpioPin = msg.gpioPin;

	switch (message)
	{
	case WM_GPIO_ON:
		log_info("Gpio " + std::to_string(gpioPin) + " triggered on!");
		TriggerOn(gpioPin);
		break;
	case WM_GPIO_OFF:
		TriggerOff(gpioPin);
		break;
	default:
		printf("Invalid gpio message.\n");
	}

	return;
}

DWORD GpioMessageProcThread(LPVOID lpParam)
{
	struct gpioMsg msg;

	while (true)
	{
		GpioMessageQueue.wait(msg);
		GpioMsgProc(msg);
	}

	return 0;
}

DWORD HttpServer(LPVOID lpParam)
{
	httplib::Server svr;

	svr.Post("/alarm", [](const httplib::Request& req, httplib::Response& res) {
		log_info("Current device test failed, alarm!");
        GPIO* deviceGPIO = dynamic_cast<GPIO*>(deviceMap.find("DC500001")->second);
        deviceGPIO->SetPinLevel(5, 1);
        std::this_thread::sleep_for(std::chrono::seconds(3));
        deviceGPIO->SetPinLevel(5, 0);

        res.set_content(req.body, "application/json");
    });

	svr.Post("/notify", [](const httplib::Request& req, httplib::Response& res) {
		log_info("Pipeline configuration modified, reload!");
		// 检查参数中是否有"flag"
		nlohmann::json jsonobj = nlohmann::json::parse(req.body);

		//		std::string pipeconfig = jsonobj["data"];
		//		nlohmann::json jsonobj1 = nlohmann::json::parse(pipeconfig);
		nlohmann::json& jsonobj1 = jsonobj["data"];
		auto map = ParsePipelineConfig(jsonobj1);
		std::unique_lock<std::mutex> lock(map_mutex);
		productMap = map;
		lock.unlock();
		std::string configPath = projDir.c_str();
		configPath.append("\\productconfig\\pipelineConfig.json");
		std::ofstream config_file(configPath, std::ofstream::trunc);
		std::string pipeconfig = jsonobj1.dump(4);
		config_file << pipeconfig;
		config_file.close();
		res.set_content(req.body, "application/json");
		});

	svr.listen("0.0.0.0", 9090);
	return 0;
}

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPWSTR    lpCmdLine,
	_In_ int       nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

	// TODO: 在此处放置代码。
	DWORD dirLen = GetCurrentDirectoryA(0, NULL);
	projDir.reserve(dirLen);
	GetCurrentDirectoryA(dirLen, &projDir[0]);

	std::string logPath = projDir.c_str();
	logPath.append("\\logs\\rotating.txt");
	log_init("AIQIForHaier",logPath, 1048576 * 50, 3);

	// 初始化全局字符串
	LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
	LoadStringW(hInstance, IDC_AIQIFORHAIER, szWindowClass, MAX_LOADSTRING);
	MyRegisterClass(hInstance);

/*	// 执行应用程序初始化:
	if (!InitInstance(hInstance, nCmdShow))
	{
		return FALSE;
	}
*/
	HANDLE hHttpPost = CreateThread(NULL, 0, HttpPostThread, NULL, 0, NULL);
	HANDLE hHttpPost1 = CreateThread(NULL, 0, HttpPostThread, NULL, 0, NULL);
	HANDLE hGpioProc = CreateThread(NULL, 0, GpioMessageProcThread, NULL, 0, NULL);
	HANDLE hHttpServer = CreateThread(NULL, 0, HttpServer, NULL, 0, NULL);

	bool parseResult = ParseBasicConfig();
	if (false == parseResult) {
		log_error("Invalid basic configuration!");
		log_finish();
		return -1;
	}

	bool testResult = StartDeviceSelfTesting();
	if (false == testResult) {
		log_error("Device self testing failed!");
		log_finish();
		return -1;
	}

	for (int i = 0; i < 3; i++) {
		bool result = FetchPipeLineConfigFile();
		if (true == result) {
			break;
		}
	}
	
	nlohmann::json jsonObj = ReadPipelineConfig();
	if (nullptr == jsonObj) {
		log_error("Invalid pipeline configuration!");
		log_finish();
		return -1;
	}

	auto map = ParsePipelineConfig(jsonObj);
	std::unique_lock<std::mutex> lock(map_mutex);
	productMap = map;
	lock.unlock();

	f_QATESTING = true;

	HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_AIQIFORHAIER));

	MSG msg;

	// 主消息循环:
	while (GetMessage(&msg, nullptr, 0, 0))
	{
		if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	log_finish();
	return (int)msg.wParam;
}

//
//  函数: MyRegisterClass()
//
//  目标: 注册窗口类
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
	WNDCLASSEXW wcex;

	wcex.cbSize = sizeof(WNDCLASSEX);

	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = WndProc;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = hInstance;
	wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_AIQIFORHAIER));
	wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wcex.lpszMenuName = MAKEINTRESOURCEW(IDC_AIQIFORHAIER);
	wcex.lpszClassName = szWindowClass;
	wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

	return RegisterClassExW(&wcex);
}

//
//   函数: InitInstance(HINSTANCE, int)
//
//   目标: 保存实例句柄并创建主窗口
//
//   注释:
//
//        在此函数中，我们在全局变量中保存实例句柄并
//        创建和显示主程序窗口。
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
	hInst = hInstance; // 将实例句柄存储在全局变量中

	HWND hWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, hInstance, nullptr);

	if (!hWnd)
	{
		return FALSE;
	}

	hEdit = CreateWindowEx(
		WS_EX_CLIENTEDGE,
		_T("EDIT"),
		_T(""),
		WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL,
		10, 10, 640, 480,
		hWnd,
		NULL,
		hInstance,
		NULL
	);

	ShowWindow(hWnd, nCmdShow);
	UpdateWindow(hWnd);

	return TRUE;
}

//
//  函数: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  目标: 处理主窗口的消息。
//
//  WM_COMMAND  - 处理应用程序菜单
//  WM_PAINT    - 绘制主窗口
//  WM_DESTROY  - 发送退出消息并返回
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	HANDLE hdw = NULL;
	switch (message)
	{
	case WM_COMMAND:
	{
		int wmId = LOWORD(wParam);
		// 分析菜单选择:
		switch (wmId)
		{
		case IDM_ABOUT:
			DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
			break;
		case IDM_EXIT:
			DestroyWindow(hWnd);
			break;
		case ID_SELFTESTING:
//			StartSelfTesting(hWnd);
			break;
		case ID_QATESTING:
			// todo: 改为 f_QATESTING = !f_QATESTING; 并加入打勾的功能
//			f_QATESTING = true;
			break;
		case ID_ALARM:
			break;
		case ID_GETCFG:
//			GetConfig(hWnd);
			break;
		case ID_CHECKUNIT:
			break;
		case ID_GETPRODUCT:
			break;
		default:
			return DefWindowProc(hWnd, message, wParam, lParam);
		}
	}
	break;
	case WM_PAINT:
	{
		PAINTSTRUCT ps;
		HDC hdc = BeginPaint(hWnd, &ps);
		// TODO: 在此处添加使用 hdc 的任何绘图代码...
		EndPaint(hWnd, &ps);
	}
	break;
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	case WM_GPIO_ON:
//		TriggerOn(hWnd, message, wParam, lParam);
		break;
	case WM_GPIO_OFF:
//		TriggerOff(hWnd, message, wParam, lParam);
		break;
	case WM_GPIOBASEMSG:
		break;
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}

// “关于”框的消息处理程序。
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
	UNREFERENCED_PARAMETER(lParam);
	switch (message)
	{
	case WM_INITDIALOG:
		return (INT_PTR)TRUE;

	case WM_COMMAND:
		if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
		{
			EndDialog(hDlg, LOWORD(wParam));
			return (INT_PTR)TRUE;
		}
		break;
	}
	return (INT_PTR)FALSE;
}

// 向编辑栏输出日志
void AppendLog(LPCWSTR text) {
	// 获取当前文本长度，以便我们知道在哪里插入新文本
	return;
	int index = GetWindowTextLength(hEdit);
	SendMessage(hEdit, EM_SETSEL, (WPARAM)index, (LPARAM)index);
	SendMessage(hEdit, EM_REPLACESEL, 0, (LPARAM)text);
}

bool ParseBasicConfig()
{
	HWND hWnd = FindWindow(NULL, szTitle);
	std::string configfile = projDir.c_str();
	configfile.append("\\basicconfig\\basicConfig.json");
	std::ifstream jsonFile(configfile);
	if (!jsonFile.is_open()) {
		log_error("Open basic configuration file failed!");
		return false;
	}

	// 解析 json
	jsonFile.seekg(0);
	nlohmann::json jsonObj;
	try {
		jsonFile >> jsonObj;
		jsonFile.close();
		pipelineCode = jsonObj["pipelineCode"];
		serverPort = jsonObj["serverPort"];
		serverIp = jsonObj["serverIp"];
		stopFlagPin = jsonObj["stopFlagPin"];
		nlohmann::json deviceConfigList = jsonObj["deviceConfigList"];
		for (const auto& deviceConfig : deviceConfigList) {
			std::string sdeviceTypeId, sdeviceTypeName, sdeviceTypeCode, sdeviceCode, sdeviceName;
			if (deviceConfig.contains("deviceTypeId")) {
				sdeviceTypeId = deviceConfig["deviceTypeId"];
			}
			if (deviceConfig.contains("deviceTypeName")) {
				sdeviceTypeName = deviceConfig["deviceTypeName"];
			}
			if (deviceConfig.contains("deviceName")) {
				sdeviceName = deviceConfig["deviceName"];
			}
			sdeviceTypeCode = deviceConfig["deviceTypeCode"];
			sdeviceCode = deviceConfig["deviceCode"];
			switch (deviceTypeCodemap[sdeviceTypeCode]) {
				case 0: { //GPIO设备
					std::string sbios = deviceConfig["bios_id"];
					const char* cbioa = sbios.data();
					char* pbios = new char[std::strlen(cbioa) + 1];
					std::strcpy(pbios, cbioa);

					if (deviceMap.find(sdeviceCode) == deviceMap.end()) {
						GPIO* deviceGPIO = new GPIO(sdeviceTypeId, sdeviceTypeName, sdeviceTypeCode, sdeviceCode, sdeviceName, hWnd, pbios, 8, WM_GPIOBASEMSG);
						deviceMap.insert(std::make_pair(sdeviceCode, deviceGPIO));
					}
					else {
						GPIO* deviceGPIO = dynamic_cast<GPIO*>(deviceMap.find(sdeviceCode)->second);
						deviceGPIO->SetGpioParam(sdeviceTypeId, sdeviceTypeName, sdeviceTypeCode, sdeviceName, hWnd, pbios, 8, WM_GPIOBASEMSG);
					}
					break;
				}

				case 1: { //光电开关
					GPIO* deviceGPIO = NULL;
					int pin;
					if (deviceConfig.contains("deviceParamConfigList")) {
						auto deviceParamConfigList = deviceConfig["deviceParamConfigList"];

						for (auto deviceParamConfig : deviceParamConfigList) {
							switch (lightParammap[deviceParamConfig["paramCode"]]) {
							case 0://延迟开始工作时间（ms）
								break;
							case 1: {//扫码枪编码
								std::string linkedScanningGun = deviceParamConfig["paramValue"];
								if (triggerMaps.find(pin) == triggerMaps.end()) {
									//如果triggerMap对应pin脚未绑定，创建vlinkedScanningGun的vector，并插入triggerMaps
									std::vector<std::string> vlinkedScanningGun;
									vlinkedScanningGun.push_back(linkedScanningGun);
									triggerMaps.insert(std::make_pair(pin, vlinkedScanningGun));
								}
								else {
									//如果triggerMap中已经存在已绑定的码枪vector，在vector中增加一个
									triggerMaps.find(pin)->second.push_back(linkedScanningGun);
								}
								break;
							}
							case 2://pin脚 TODO: 最先处理
								pin = std::stoi((std::string)deviceParamConfig["paramValue"]);
								break;
							case 3: {//GPIO设备号*/
								std::string GPIODeviceCode = deviceParamConfig["paramValue"];
								if (deviceMap.find(GPIODeviceCode) == deviceMap.end()) {
									deviceGPIO = new GPIO(GPIODeviceCode);
									deviceMap.insert(std::make_pair(GPIODeviceCode, deviceGPIO));
									deviceGPIO->RegistDevice(pin, hWnd);
								}
								else {
									deviceGPIO = dynamic_cast<GPIO*>(deviceMap.find(GPIODeviceCode)->second);
									deviceGPIO->RegistDevice(pin, hWnd);
								}
								break;
							}
							default:
								break;
							}
						}
					}

					std::string codereaderID = "";
					peSwitch* lSwitch = new peSwitch(pin, WM_GPIOBASEMSG, codereaderID, true, deviceGPIO, hWnd, sdeviceTypeId, sdeviceTypeName, sdeviceTypeCode, sdeviceCode, sdeviceName);
					deviceMap.insert(std::make_pair(sdeviceCode, lSwitch));
					break;
				}
				case 2: { //摄像机
					std::string ipAddr = deviceConfig["ipAddr"];
					Camera* cdevice = new Camera(ipAddr, sdeviceTypeId, sdeviceTypeName, sdeviceTypeCode, sdeviceCode, sdeviceName);
					if (deviceConfig.contains("deviceParamConfigList")) {
						cdevice->SetValuesByJson(deviceConfig["deviceParamConfigList"]);
					}
					deviceMap.insert(std::make_pair(sdeviceCode, cdevice));
					break;
				}
				case 3: { //扫码枪
					std::string ipAddr = deviceConfig["ipAddr"];
					CodeReader* cddevice = new CodeReader(ipAddr, sdeviceTypeId, sdeviceTypeName, sdeviceTypeCode, sdeviceCode, sdeviceName);
					if (deviceConfig.contains("deviceParamConfigList")) {
						cddevice->SetValuesByJson(deviceConfig["deviceParamConfigList"]);
					}
					deviceMap.insert(std::make_pair(sdeviceCode, cddevice));
					break;
				}
				case 4: { //音频设备
					AudioEquipment* audioDevice = new AudioEquipment(sdeviceTypeId, sdeviceTypeName, sdeviceTypeCode, sdeviceCode, sdeviceName);
					deviceMap.insert(std::make_pair(sdeviceCode, audioDevice));
					break;
				}
				case 5: { //遥控器
					std::string portName = "COM3";
					int baudRate = 38400;
					std::string message = "FF 16 06 16 00 32 A6 EA 00 00 C0 20 00 80 80 00 00 00 0B 7B B7 00 40 00 00 00 00 F7 A6 EA 00 00 C0 20 00 80 00 00 00 00 0B FB B7 00 40 00 00 00 00 F7 A6 EA 00 00 C0 20 00 80 80 00 00 00 0B 7B B7 00 40 00 00 00 00 F7 A6 EA 00 00 C0 20 00 80 00 00 00 00 0B FB B7 00 40 00 00 00 00 F7 A6 EA 00 00 C0 20 00 80 80 00 00 00 0B 7B B7 00 40 00 00 00 00 F7 A6 EA 00 00 C0 20 00 80 00 00 00 00 0B FB B7 00 40 00 00 00 00 F7 AD 84";
					int devicePin = 0;
					if (deviceConfig.contains("deviceParamConfigList")) {
						auto deviceParamConfigList = deviceConfig["deviceParamConfigList"];
						for (auto deviceParamConfig : deviceParamConfigList) {
							switch (RemoteControlParammap[deviceParamConfig["paramCode"]]) {
							case 0:
								// devicelatency
								break;
							case 1:
								// baudRate
								baudRate = std::stoi((std::string)deviceParamConfig["paramValue"]);
								break;
							case 2:
								portName = deviceParamConfig["paramValue"];
								break;
							case 3:
								message = deviceParamConfig["paramValue"];
								break;
							case 4:
								devicePin = remoteCtrlPin = std::stoi((std::string)deviceParamConfig["paramValue"]);
							default:
								break;
							}
						}

					}
					SerialCommunication* scDevice = new SerialCommunication(sdeviceTypeId, sdeviceTypeName, sdeviceTypeCode, sdeviceCode, sdeviceName, portName, baudRate, message, devicePin);
					deviceMap.insert(std::make_pair(sdeviceCode, scDevice));
					break;
				}
				case 6: { //音频设备
					AudioEquipment* audioDevice = new AudioEquipment(sdeviceTypeId, sdeviceTypeName, sdeviceTypeCode, sdeviceCode, sdeviceName);
					deviceMap.insert(std::make_pair(sdeviceCode, audioDevice));
					break;
				}
				default:
				    break;
			}
		}
	}
	catch (const nlohmann::json::parse_error& e) {
		std::string errinfo = "Exception occur while parse basic config!, exception msg:";
		errinfo.append(e.what());
		log_error(errinfo);
		return false;
	}

	return true;
}

// 各个按钮对应的函数
bool StartDeviceSelfTesting() {
	int testCount = 0;
	for (auto it = deviceMap.begin(); it != deviceMap.end(); ++it) {
		switch (deviceTypeCodemap[it->second->getDeviceTypeCode()]) {
		case 0: {//GPIO
			GPIO* deviceGPIO = dynamic_cast<GPIO*>(it->second);
			if (deviceGPIO->Init() && deviceGPIO->StartThread()) {
				log_info("GPIO "  + deviceGPIO->getDeviceCode() + " self test successed!");
				testCount++;
			}
			else {
				log_error("GPIO " + deviceGPIO->getDeviceCode() + " self test failed!");
			}
			break;
		}
		case 1: //光电开关自检默认通过
			log_info("Light switch self test successed!");
			testCount++;
			break;
		case 2: {//摄像机开关
			Camera* deviceCamera = dynamic_cast<Camera*>(it->second);
			if (deviceCamera->GetCameraByIpAddress() && deviceCamera->Init()) {
				deviceCamera->StartGrabbing();
				deviceCamera->GetImageX();
				log_info("Camera " + deviceCamera->getDeviceCode() + " self test successed!");
				testCount++;
			}
			else {
				log_error("Camera " + deviceCamera->getDeviceCode() + " self test failed!");
			}
			break;
		}
		case 3: {//码枪
			CodeReader* deviceCodeReader = dynamic_cast<CodeReader*>(it->second);
			if (deviceCodeReader->GetCodeReaderByIpAddress() && deviceCodeReader->Init()) {
				deviceCodeReader->StartGrabbing();
				log_info("Code Reader " + deviceCodeReader->getDeviceCode() + " self test successed!");
				testCount++;
			}
			else {
				log_error("Code Reader " + deviceCodeReader->getDeviceCode() + " self test failed!");
			}
			break;
		}
		case 4: { //扬声器speaker
			AudioEquipment* audioDevice = dynamic_cast<AudioEquipment*>(it->second);
				
			std::string soundFilePath = projDir.c_str();
			soundFilePath.append("\\sounds\\");
			audioDevice->ReadFile(soundFilePath);

			//int ret = audioDevice->Init(soundFile); // todo: 解决未找到音频设备时抛出异常的问题
			log_info("Speaker " + audioDevice->getDeviceCode() + " self test successed!");
			testCount++; // todo: 加条件
			break;
		}
		case 5: { // 红外发射器
			SerialCommunication* scDevice = dynamic_cast<SerialCommunication*>(it->second);
			//if (scDevice->OpenSerial()) {
			log_info("IR transmitter " + scDevice->getDeviceCode() + " self test successed!");
			testCount++;
			//}
			//else {
			//	std::string logStr = "Remote control " + it->first + " selftest error!\n";
			//	AppendLog(StringToLPCWSTR(logStr));
			//}
			break;
		}
		case 6: {
			AudioEquipment* audioDevice = dynamic_cast<AudioEquipment*>(it->second);
			log_info("Speaker " + audioDevice->getDeviceCode() + " self test successed!");
			testCount++;
			break;
		}
		default:
			break;
		}
	}

	if (testCount != deviceMap.size()) {
		return false;
	}

	return true;
}

// 处理得到 productMap
bool FetchPipeLineConfigFile() {
	// 调用 GetPipelineConfig.jar
	std::string jarPath = projDir.c_str();
	jarPath.append("\\GetPipelineConfig.jar");
	std::string cfgDir = projDir.c_str();
	cfgDir.append("\\productconfig\\");
	std::string baseUrl = "http://" + serverIp + ":" + std::to_string(serverPort) + "/api/client";
	std::string command = "start /b java -jar " + jarPath + " " + baseUrl + " " + pipelineCode + " " + cfgDir + " " + "pipelineConfig.json";
	std::system(command.c_str());

	// 检查 flag
	std::string flagpath = projDir.c_str();
	flagpath.append("\\productconfig\\flag");
	auto now = std::chrono::system_clock::now();
	auto duration_in_seconds = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch());
	while (INVALID_FILE_ATTRIBUTES == GetFileAttributes(StringToWstring(flagpath).c_str())) {
		now = std::chrono::system_clock::now();
		auto duration_now_in_seconds_now = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch());
		if ((duration_now_in_seconds_now.count() - duration_in_seconds.count()) > 20) {
			log_info("get pipeline configuration failed!");
			return false;
		}
		Sleep(1000);
	}

	remove(flagpath.c_str());
	return true;
}

nlohmann::json ReadPipelineConfig() {
	// 读取 pipelineConfig.json
	std::string configfile = projDir.c_str();
	configfile.append("\\productconfig\\pipelineConfig.json");
	std::ifstream jsonFile(configfile);
	if (!jsonFile.is_open()) {
		log_error("pipeline configuration file open failed!");
		return nullptr;
	}

	// 解析 json
	jsonFile.seekg(0);
	nlohmann::json jsonObj;
	try {
		jsonFile >> jsonObj;
		jsonFile.close();
	}
	catch (const nlohmann::json::parse_error& e) {
		log_error("pipeline configuration parse failed!");
		return nullptr;
	}

	return jsonObj;
}

std::shared_ptr<std::unordered_map<std::string, std::shared_ptr<Product>>> ParsePipelineConfig(nlohmann::json& jsonObj) {
	auto productMaps = std::make_shared<std::unordered_map<std::string, std::shared_ptr<Product>>>();
    try {
		// 获取pipeline基本信息
		pipelineCode = jsonObj["pipelineCode"];
		pipelineName = jsonObj["pipelineName"];
		auto productConfigList = jsonObj["productConfigList"];
		for (auto productConfig : productConfigList) {
			// product
			std::string productName = productConfig["productName"];
			std::string productSnCode = productConfig["productSnCode"]; //前9个字符，码枪扫出的
			productSnCode = productSnCode.substr(0, 9);
			std::string productSnModel = productConfig["productSnModel"];
			std::string audioFileName = "xiaoyouxiaoyou";
			if (productConfig.contains("audioFileName"))
			{
				audioFileName = productConfig["audioFileName"];			
			}

			auto tmpProduct = std::make_shared<Product>(productSnCode, productName, productSnModel);

			// processesMap and testListMap
			auto processesMap = new std::unordered_map<int, std::vector<std::string>*>();
			auto testListMap = new std::unordered_map<int, ProcessUnit* >();
			for (int pinNum = 0; pinNum < 8; pinNum++) {
				std::vector<std::string>* processVector = new std::vector<std::string>();
				processesMap->insert(std::make_pair(pinNum, processVector));

				ProcessUnit* processListHead = new ProcessUnit();
				processListHead->nextunit = processListHead->prevunit = processListHead;
				processListHead->pin = pinNum;
				testListMap->insert(std::make_pair(pinNum, processListHead));

				// 初始化 map2bTest
				product2btest* p2btstNull = new product2btest();
				p2btstNull->pinNumber = pinNum;
				map2bTest.insert(std::make_pair(pinNum, p2btstNull));
			}
			tmpProduct->SetProcessCodeMap(processesMap);
			tmpProduct->SetTestListMap(testListMap);

			auto processesConfigList = productConfig["processesConfigList"];
			for (auto processes : processesConfigList) {
				std::string processesCode = processes["processesCode"];
				std::string processesTemplateCode = processes["processesTemplateCode"];
				std::string processesTemplateName = processes["processesTemplateName"];
				auto deviceConfigList = processes["deviceConfigList"];

				int cnt = 0;
				for (auto deviceCfg : deviceConfigList) {
					ProcessUnit* curUnit = new ProcessUnit();
					curUnit->processesCode = processesCode;
					curUnit->processesTemplateCode = processesTemplateCode;
					curUnit->processesTemplateName = processesTemplateName;
					curUnit->productName = productName;
					curUnit->productSnCode = productSnCode;
					curUnit->productSnModel = productSnModel;
					int gpioPin;
					switch (deviceTypeCodemap[deviceCfg["deviceTypeCode"]]) {
					case 0: // 此处无 GPIO
						break;
					case 1: { //获取绑定的 pin 和扫码枪，光电开关
						auto deviceParamConfigList = deviceCfg["deviceParamConfigList"];
						for (auto deviceParam : deviceParamConfigList) {
							switch (lightParammap[deviceParam["paramCode"]]) {
							case 0: // 延迟开始工作时间（ms）
								break;
							case 1: { // 扫码枪编码 todo: 检查需在读取 pin 脚后
								//std::string codeReaderDeviceCode = (std::string)deviceParam["paramValue"];
								////if (!triggerMap[gpioPin]._Equal(codeReaderDeviceCode)) {
								////	AppendLog(_T("警告：配置中的光电开关绑定的 GPIO pin 和预设值不一样！\n"));
								////}
								//std::vector<std::string> codeReaderDeviceCodes = triggerMaps[gpioPin];
								//int cnt = 0;
								//for (std::string code : codeReaderDeviceCodes) {
								//	if (codeReaderDeviceCode._Equal(code)) {
								//		cnt++;
								//	}
								//}
								//if (0 == cnt) {
								//	AppendLog(_T("警告：配置中的光电开关绑定的 GPIO pin 和预设值不一样！\n"));
								//}
								break;
							}
							case 2: { // pin脚
								gpioPin = std::stoi((std::string)deviceParam["paramValue"]);
								std::vector<std::string>* tmpProcessVector = processesMap->find(gpioPin)->second;
								tmpProcessVector->push_back(processesCode);
								break;
							}
							case 3: { // GPIO设备号 todo: 写死
								std::string gpioDeviceCode = (std::string)deviceParam["paramValue"];
								auto it = deviceMap.find(gpioDeviceCode);
								if (it->second->e_deviceTypeCode != "GPIO") {
									AppendLog(_T("警告：配置中的 GPIO code 和预设值不一样！\n"));
								}
								break;
							}
							default:
								break;
							}
						}
						break;
					}
					case 2: { //camera
						curUnit->deviceTypeCode = deviceCfg["deviceTypeCode"];
						curUnit->deviceCode = deviceCfg["deviceCode"];
						if (curUnit->deviceCode == "DC100011") {
						}
						curUnit->eq = deviceMap[curUnit->deviceCode];
						curUnit->parameter = deviceCfg["deviceParamConfigList"];
						auto deviceParamConfigList = deviceCfg["deviceParamConfigList"];
						for (auto deviceParam : deviceParamConfigList) {
							switch (CameraParammap[deviceParam["paramCode"]]) {
							case 0:
								break;
							case 1:
								break;
							case 2:
								break;
							case 3:
								curUnit->laterncy = std::stol((std::string)deviceParam["paramValue"]);
								break;
							case 4:
								break;
							case 5:
								break;
							default:
								break;
							}
						}
						ProcessUnit* processListHead = testListMap->find(gpioPin)->second; // todo: gpioPin若不存在，需初始化
						insertProcessUnit(processListHead, curUnit);
						break;
					}
					case 3: { //扫码枪
						curUnit->deviceTypeCode = deviceCfg["deviceTypeCode"];
						curUnit->deviceCode = deviceCfg["deviceCode"];

						// 跳过光电开关绑定的扫码枪
						//if (curUnit->deviceCode == triggerMap[gpioPin]) {
						//	
						//	delete curUnit;
						//	break;
						//}

						for (auto& deviceCode : triggerMaps[gpioPin]) {
							if (curUnit->deviceCode == deviceCode) {
								delete curUnit;
								curUnit = nullptr;
								break;
							}
						}
						if (curUnit == nullptr) {
							break;
						}

						curUnit->eq = deviceMap[curUnit->deviceCode];
						curUnit->parameter = deviceCfg["deviceParamConfigList"];
						auto deviceParamConfigList = deviceCfg["deviceParamConfigList"];
						for (auto deviceParam : deviceParamConfigList) {
							switch (ScanningGunParammap[deviceParam["paramCode"]]) {
							case 0:
								break;
							case 1:
								curUnit->laterncy = std::stol((std::string)deviceParam["paramValue"]);
								break;
							case 2:
								break;
							case 3:
								break;
							case 4:
								break;
							case 5:
								break;
							case 6:
								break;
							default:
								break;
							}
						}
						ProcessUnit* processListHead = testListMap->find(gpioPin)->second;
						insertProcessUnit(processListHead, curUnit);
						break;
					}
					case 4: {//Speaker设备
						curUnit->audioFileName = audioFileName;
						curUnit->deviceTypeCode = deviceCfg["deviceTypeCode"];
						curUnit->deviceCode = deviceCfg["deviceCode"];
						curUnit->eq = deviceMap[curUnit->deviceCode];
						curUnit->parameter = deviceCfg["deviceParamConfigList"];
						auto deviceParamConfigList = deviceCfg["deviceParamConfigList"];
						for (auto deviceParam : deviceParamConfigList) {
							if ((std::string)deviceParam["paramCode"] == "devicelatency") {
								curUnit->laterncy = std::stol((std::string)deviceParam["paramValue"]);
							}
						}
						ProcessUnit* processListHead = testListMap->find(gpioPin)->second;
						insertProcessUnit(processListHead, curUnit);
						break;
					}
					case 5: // todo: 加串口
						break;
					case 6: {//Recorder设备
						curUnit->deviceTypeCode = deviceCfg["deviceTypeCode"];
						curUnit->deviceCode = deviceCfg["deviceCode"];
						curUnit->eq = deviceMap[curUnit->deviceCode];
						curUnit->parameter = deviceCfg["deviceParamConfigList"];
						auto deviceParamConfigList = deviceCfg["deviceParamConfigList"];
						for (auto deviceParam : deviceParamConfigList) {
							if ((std::string)deviceParam["paramCode"] == "devicelatency") {
								curUnit->laterncy = std::stol((std::string)deviceParam["paramValue"]);
							}
						}
						ProcessUnit* processListHead = testListMap->find(gpioPin)->second;
						insertProcessUnit(processListHead, curUnit);
						break;
					}
					default:
						break;
					}
				}
			}
			productMaps->insert(std::make_pair(productSnCode, tmpProduct));
		}
	}
	catch (const nlohmann::json::parse_error& e) {
		f_GETCFG = false;
		log_error("pipeline configuration parse failed!");
	}

	return productMaps;
}

DWORD __stdcall InfraredRemoteCtlThread(LPVOID lpParam) {
	CloseHandle(GetCurrentThread());
	log_info("Infrared remote controler be called!");
	WZSerialPort w;

	char cmdI[] = { 0xFF,0x16,0x06,0x16,0x00,0x32,0xA6,0xEA,0x00,0x00,0xC0,0x20,0x00,0x80,0x80,0x00,0x00,0x00,0x0B, \
	0x7B,0xB7,0x00,0x40,0x00,0x00,0x00,0x00,0xF7,0xA6,0xEA,0x00,0x00,0xC0,0x20,0x00,0x80,0x00,0x00,0x00,0x00,0x0B, \
	0xFB,0xB7,0x00,0x40,0x00,0x00,0x00,0x00,0xF7,0xA6,0xEA,0x00,0x00,0xC0,0x20,0x00,0x80,0x80,0x00,0x00,0x00,0x0B, \
	0x7B,0xB7,0x00,0x40,0x00,0x00,0x00,0x00,0xF7,0xA6,0xEA,0x00,0x00,0xC0,0x20,0x00,0x80,0x00,0x00,0x00,0x00,0x0B, \
	0xFB,0xB7,0x00,0x40,0x00,0x00,0x00,0x00,0xF7,0xA6,0xEA,0x00,0x00,0xC0,0x20,0x00,0x80,0x80,0x00,0x00,0x00,0x0B, \
	0x7B,0xB7,0x00,0x40,0x00,0x00,0x00,0x00,0xF7,0xA6,0xEA,0x00,0x00,0xC0,0x20,0x00,0x80,0x00,0x00,0x00,0x00,0x0B, \
	0xFB,0xB7,0x00,0x40,0x00,0x00,0x00,0x00,0xF7,0xAD,0x84 };

	char cmdG[] = { 0xFF, 0x16, 0x06, 0x14, 0x00, 0x32, 0xA6, 0xEC, 0x00, 0x00, 0xC0, 0x20, 0x00, 0x80, 0x80, 0x00, \
		0x00, 0x00, 0x0B, 0x7D, 0xB5, 0x00, 0x40, 0x00, 0x00, 0xF5, 0xA6, 0xEC, 0x00, 0x00, 0xC0, 0x20, 0x00, 0x80, \
		0x00, 0x00, 0x00, 0x00, 0x0B, 0xFD, 0xB5, 0x00, 0x40, 0x00, 0x00, 0xF5, 0xA6, 0xEC, 0x00, 0x00, 0xC0, 0x20, \
		0x00, 0x80, 0x80, 0x00, 0x00, 0x00, 0x0B, 0x7D, 0xB5, 0x00, 0x40, 0x00, 0x00, 0xF5, 0xA6, 0xEC, 0x00, 0x00, \
		0xC0, 0x20, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x0B, 0xFD, 0xB5, 0x00, 0x40, 0x00, 0x00, 0xF5, 0xA6, 0xEC, \
		0x00, 0x00, 0xC0, 0x20, 0x00, 0x80, 0x80, 0x00, 0x00, 0x00, 0x0B, 0x7D, 0xB5, 0x00, 0x40, 0x00, 0x00, 0xF5, \
		0xA6, 0xEC, 0x00, 0x00, 0xC0, 0x20, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x0B, 0xFD, 0xB5, 0x00, 0x40, 0x00, 0x00, 0xF5, 0x6C, 0x63
	};

	char cmdH[] = { 0xFF, 0x16, 0x06, 0x14, 0x00, 0x0A, 0xA6, 0xEC, 0xE0, 0x00, 0xC0, 0x20, 0x00, 0x80, 0x80, 0x00, \
		0x00, 0x00, 0x8B, 0xDD, 0xB5, 0x00, 0x20, 0x00, 0x00, 0xD5, 0xA6, 0xEC, 0xE0, 0x00, 0xC0, 0x20, 0x00, 0x80, \
		0x00, 0x00, 0x00, 0x00, 0x8B, 0x5D, 0xB5, 0x00, 0x20, 0x00, 0x00, 0xD5, 0xA6, 0xEC, 0xE0, 0x00, 0xC0, 0x20, \
		0x00, 0x80, 0x80, 0x00, 0x00, 0x00, 0x8B, 0xDD, 0xB5, 0x00, 0x20, 0x00, 0x00, 0xD5, 0xA6, 0xEC, 0xE0, 0x00, \
		0xC0, 0x20, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x8B, 0x5D, 0xB5, 0x00, 0x20, 0x00, 0x00, 0xD5, 0xA6, 0xEC, \
		0xE0, 0x00, 0xC0, 0x20, 0x00, 0x80, 0x80, 0x00, 0x00, 0x00, 0x8B, 0xDD, 0xB5, 0x00, 0x20, 0x00, 0x00, 0xD5, \
		0xA6, 0xEC, 0xE0, 0x00, 0xC0, 0x20, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x8B, 0x5D, 0xB5, 0x00, 0x20, 0x00, 0x00, 0xD5, 0x03, 0xDC
	};

	unsigned char rcvBuf[2048] = { 0 };
	if (w.open("com1"))
	{
		cout << "open success!" << std::endl;
		w.sendBytes(cmdI, sizeof(cmdI));
/*		Sleep(5000);
		int length = w.receive(rcvBuf, sizeof(rcvBuf));
		for (int i = 0; i < length; i++)
		{
			printf("0x%x ", rcvBuf[i]);
		}
		printf("\n");
*/
		w.close();
	}
	return 0;
}

struct UnitWorkPara
{
	bool sameProductSnCode;
	ProcessUnit* procUnit;
};

DWORD __stdcall MainWorkThread(LPVOID lpParam) {
	CloseHandle(GetCurrentThread());
	int gpioPin = (int)lpParam;

	if (gpioPin == 4)
	{
		CreateThread(NULL, 0, InfraredRemoteCtlThread, NULL, 0, NULL);
		return 0;
	}

	log_info("Gpiopin " + std::to_string(gpioPin) + ": Start scan product sn code!");
	// 得到产品序列号前9位
	std::vector<std::string> vcodereaders = triggerMaps[gpioPin];
	std::vector<std::string> codereaderresults;
	for (std::string codereader : vcodereaders) {
		auto it = deviceMap.find(codereader);
		if (it == deviceMap.end()) {
			log_warn("Gpiopin " + std::to_string(gpioPin) + ": Light switch doesn't bind scancoder, please check configure!");
			return 0;
		}
		CodeReader* CR = dynamic_cast<CodeReader*>(it->second);
		log_info("Gpiopin " + std::to_string(gpioPin) + ": CodeReader " + CR->e_deviceCode + " called!");
		std::vector<std::string> results;
		CR->Lock();
		int crRet = CR->ReadCode(results);
		CR->UnLock();
		codereaderresults.insert(codereaderresults.end(), results.begin(), results.end());
		if (!results.empty())
		{
			break;
		}
	}

	if (codereaderresults.empty()) {
		log_warn("Gpiopin " + std::to_string(gpioPin) + ": Scan product sn code result is null!");
		return 0;
	}
	std::string productSn = "";
	for (size_t i = 0; i < codereaderresults.size(); i++) {
		std::string tmpr = codereaderresults[i];
		// 取最长
		if (tmpr.size() > productSn.size()) {
			productSn = tmpr;
		}
	}
	if (productSn.length() < 13) {
		log_warn("Gpiopin " + std::to_string(gpioPin) + ": Scan  product sn code failed, product sn length less than 13!");
		return 0;
	}
	log_info("Gpiopin " + std::to_string(gpioPin) + ": End scan product sn code!");

	std::string productSnCode = productSn.substr(0, 9);

	std::unique_lock<std::mutex> lock(map_mutex);
	// 跳过 pipeline 配置里不存在的商品总成号 productSnCode
	auto productItem = productMap->find(productSnCode);
	if (productItem == productMap->end()) {
		log_warn("Gpiopin " + std::to_string(gpioPin) + ": Product sn " + productSnCode + " does not exist!");
		return 0;
	}
	auto product = productItem->second;
	lock.unlock();

	log_info("Gpiopin " + std::to_string(gpioPin) + ": Product sn " + productSn + " Scaned!");

	ProcessUnit* head =product->testListMap->find(gpioPin)->second;

	auto now0 = std::chrono::system_clock::now();
	auto duration_in_milliseconds0 = std::chrono::duration_cast<std::chrono::milliseconds>(now0.time_since_epoch());
	long startTime = duration_in_milliseconds0.count();

	auto it = map2bTest.find(gpioPin);
	if (it == map2bTest.end()) {
		log_warn("Gpiopin " + std::to_string(gpioPin) + ": Map2bTest init error!");
		return 0;
	}
    std::string lastProductSnCode = it->second->productSnCode;
	it->second->pinNumber = gpioPin;
	it->second->productSn = productSn;
	it->second->productSnCode = productSnCode;
	it->second->processUnitListHead = head;
	it->second->shotTimestamp = startTime;

	product2btest* myp2btest = it->second;
	std::vector<HANDLE> handles;
	auto now1 = std::chrono::system_clock::now();
	auto duration_in_milliseconds1 = std::chrono::duration_cast<std::chrono::milliseconds>(now1.time_since_epoch());
	long start = duration_in_milliseconds1.count();

	ProcessUnit* tmpFind = myp2btest->processUnitListHead->nextunit;
	int count = 0;
	int gpiopin = myp2btest->processUnitListHead->pin;
	while (tmpFind != myp2btest->processUnitListHead) {
		auto now2 = std::chrono::system_clock::now();
		auto duration_in_milliseconds2 = std::chrono::duration_cast<std::chrono::milliseconds>(now2.time_since_epoch());
		long runla = duration_in_milliseconds2.count();

		// 加句柄队列
		if ((runla - start) >= tmpFind->laterncy) {
			tmpFind->productSn = myp2btest->productSn;
			struct UnitWorkPara* param = new(struct UnitWorkPara);
			if (lastProductSnCode == productSnCode)
			{
				param->sameProductSnCode = TRUE;
			}
			else
			{
				param->sameProductSnCode = FALSE;
			}
			param->procUnit = tmpFind;
			HANDLE hUnitWork = CreateThread(NULL, 0, UnitWorkThread, param, 0, NULL);
			handles.push_back(hUnitWork);
			tmpFind = tmpFind->nextunit;
		}
		else {
			Sleep(10);
		}
	}

	for (auto& handle : handles) {
		WaitForSingleObject(handle, INFINITE);
		CloseHandle(handle);
	}

	/*最后一个gpio引脚触发事件处理结束后，发送检测结束标志*/
	if (gpioPin == 1)
	{
		Sleep(1500);
		struct httpMsg msg;
		Counter.mutex.lock();
		Counter.count++;
		msg.msgId = Counter.count;
		Counter.mutex.unlock();
		msg.pipelineCode = pipelineCode;
		msg.productSn = productSn + pipelineCode;
		msg.type = MSG_TYPE_STOP;

		Singleton::instance().push(msg);
		log_info("push stop msg, msgId: " + std::to_string(msg.msgId) + ", processSn: " + msg.productSn);
	}
	return 0;
}

void TriggerOn(UINT gpioPin)
{
	if (!f_QATESTING) {
		return;
	}

	// 1. triggerMap 把引脚和扫码枪对应
	// 2. 根据扫码枪扫到的码，确定产品
	// 3. 根据产品确定执行的 process
	if (!mtx[gpioPin].try_lock()) {
		return;
	}

	if (isPinTriggered[gpioPin]) {
		mtx[gpioPin].unlock();
		return;
	}
	isPinTriggered[gpioPin] = true;

	HANDLE hdw = CreateThread(NULL, 0, MainWorkThread, (LPVOID)gpioPin, 0, NULL);
	return;
}

DWORD __stdcall UnitWorkThread(LPVOID lpParam) {
	struct UnitWorkPara *param = static_cast<struct UnitWorkPara *>(lpParam);
	ProcessUnit* unit = param->procUnit;
	bool sameProductSn = param->sameProductSnCode;
	delete(param);
	std::string path = projDir.c_str();

	switch (deviceTypeCodemap[unit->deviceTypeCode]) {
	case 2: { // Camera
		Camera* devicecm = dynamic_cast<Camera*>(unit->eq);
		log_info("Camera " + devicecm->e_deviceCode + " be called!");
		devicecm->Lock();
		if (sameProductSn == FALSE)
		{
			devicecm->SetValuesByJson(unit->parameter);
		}
		devicecm->GetImage(path, unit);
		devicecm->UnLock();
		break;
	}
	case 3: { // ScanningGun
		CodeReader* deviceCR = dynamic_cast<CodeReader*>(unit->eq);
		log_info("Scancoder " + deviceCR->e_deviceCode + " be called!");

		deviceCR->Lock();
		if (sameProductSn == FALSE)
		{
			deviceCR->SetValuesByJson(unit->parameter);
		}
		std::vector<std::string> codeRes;
		int crRet = deviceCR->ReadCode(codeRes);
		deviceCR->UnLock();

		struct httpMsg msg;
		Counter.mutex.lock();
		Counter.count++;
		msg.msgId = Counter.count;
		Counter.mutex.unlock();
		msg.pipelineCode = pipelineCode;
		msg.processesCode = unit->processesCode;
		msg.processesTemplateCode = unit->processesTemplateCode;
		msg.productSn = unit->productSn + pipelineCode;
		msg.productSnCode = unit->productSnCode;
		msg.productSnModel = unit->productSnModel;
		msg.type = MSG_TYPE_TEXT;
		if (!codeRes.empty()) {
			msg.text = "";
			for (size_t i = 0; i < codeRes.size(); ++i)
			{
				if (codeRes[i].length() > 0)
				{
					msg.text = codeRes[i];
					break;
				}
			}
		}
		else {
			msg.text = "";
		}
		Singleton::instance().push(msg);
		log_info("push msg, msgId: " + std::to_string(msg.msgId) + ", processSn: " + msg.productSn + ", processesTemplateCode : " + msg.processesTemplateCode);
		break;
	}
	case 4: { // Speaker
		AudioEquipment* audioDevice = dynamic_cast<AudioEquipment*>(unit->eq);
		log_info("Speaker device " + audioDevice->e_deviceCode + " be called!");
/*		// 初始化
		int ret = audioDevice->Init(); // todo: 解决未找到音频设备时抛出异常的问题

		auto now = std::chrono::system_clock::now();
		auto duration = now.time_since_epoch();
		auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
	    std:string outFile = path + "\\temp\\" + std::to_string(milliseconds) + ".pcm";

		// 录制 todo: 考虑录制时间
		std::string recordFile = path + "\\temp"+ "\\audio_data.pcm";
		std::string logStr = "Record: ";
		logStr.append(recordFile).append("\n");
		AppendLog(StringToLPCWSTR(logStr));

		int audioRet = audioDevice->PlayAndRecord(recordFile, 7);
		logStr = "Ret: ";
		logStr.append(std::to_string(audioRet)).append("\n");
		AppendLog(StringToLPCWSTR(logStr));

		audioDevice->To16k(recordFile);
		std::string recordFile1 = path + "\\temp" + "\\audio_data_16.pcm";
		audioDevice->CutFile(recordFile1, outFile, 6, 7);

		// 删除文件
		deleteFile(recordFile);
		audioDevice->Terminate();
		if (0 == audioRet)
		{
			struct httpMsg msg;
			msg.pipelineCode = pipelineCode;
			msg.processesCode = unit->processesCode;
			msg.processesTemplateCode = unit->processesTemplateCode;
			msg.productSn = unit->productSn;
			msg.productSnCode = unit->productSnCode;
			msg.productSnModel = unit->productSnModel;
			msg.type = MSG_TYPE_SOUND;
			msg.sampleTime = milliseconds;
			Singleton::instance().push(msg);
		}
*/
		WAVEFORMATEX format;
		format.wFormatTag = WAVE_FORMAT_PCM;
		format.nChannels = 1;
		format.nSamplesPerSec = 48000;
		format.wBitsPerSample = 16;
		format.nBlockAlign = (format.nChannels * format.wBitsPerSample) / 8;
		format.nAvgBytesPerSec = format.nSamplesPerSec * format.nBlockAlign;
		format.cbSize = 0;

		audioDevice->PlayAudio(&format, unit->audioFileName);
		break;
	}
	case 5: { // RemoteControl
		// todo: 加串口
		break;
	}
	case 6: { //Microphone
		AudioEquipment* audioDevice = dynamic_cast<AudioEquipment*>(unit->eq);
		log_info("Microphone device " + audioDevice->e_deviceCode + " be called!");

		WAVEFORMATEX waveFormat;
		waveFormat.wFormatTag = WAVE_FORMAT_PCM;
		waveFormat.nSamplesPerSec = 48000;
		waveFormat.nChannels = 1;
		waveFormat.wBitsPerSample = 16;
		waveFormat.nBlockAlign = (waveFormat.nChannels * waveFormat.wBitsPerSample) / 8;
		waveFormat.nAvgBytesPerSec = waveFormat.nSamplesPerSec * waveFormat.nBlockAlign;
		waveFormat.cbSize = 0;

		auto now = std::chrono::system_clock::now();
		auto duration = now.time_since_epoch();
		auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();

		std::string recordFile = path + "\\temp" + "\\audio_data.pcm";
		int audioRet = audioDevice->RecordAudio(&waveFormat, 2, recordFile);
		std::string resultFile = path + "\\temp\\" + std::to_string(milliseconds) + ".pcm";
		audioDevice->To16k(recordFile, resultFile);

		// 删除文件
		deleteFile(recordFile);

		if (0 == audioRet)
		{
			struct httpMsg msg;
			Counter.mutex.lock();
			Counter.count++;
			msg.msgId = Counter.count;
			Counter.mutex.unlock();
			msg.pipelineCode = pipelineCode;
			msg.processesCode = unit->processesCode;
			msg.processesTemplateCode = unit->processesTemplateCode;
			msg.productSn = unit->productSn + pipelineCode;
			msg.productSnCode = unit->productSnCode;
			msg.productSnModel = unit->productSnModel;
			msg.type = MSG_TYPE_SOUND;
			msg.sampleTime = milliseconds;
			Singleton::instance().push(msg);
			log_info("push msg, msgId: " + std::to_string(msg.msgId) + ", processSn: " + msg.productSn + ", processesTemplateCode : " + msg.processesTemplateCode);
		}
		break;
	}
	default:
		break;
	}

	return 0;
}

void TriggerOff(UINT gpioPin)
{
	if (!f_QATESTING) {
		isPinTriggered[gpioPin] = false; // todo: check
		return;
	}

	mtx[gpioPin].try_lock();
	mtx[gpioPin].unlock();
	isPinTriggered[gpioPin] = false;

	return;
}
