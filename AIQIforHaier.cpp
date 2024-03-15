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
#define HTTP_POST_PORT 10001

std::mutex mtx[8];
bool isPinTriggered[8]; // GPIO 针脚是否触发状态
HANDLE hMainWork[8]; // 当前线程句柄
std::list<HANDLE> allMainWorkHandles; // 所有线程句柄
int remoteCtrlPin;

std::string baseUrl = "http://10.142.193.10:10001";
std::string pipelineCode = "CX202401261510000001"; // 一台工控机只跑一个 pipeline
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
DWORD __stdcall CheckAndClearLog(LPVOID lpParam);
DWORD __stdcall MainWorkThread(LPVOID lpParam);
DWORD __stdcall UnitWorkThread(LPVOID lpParam);

struct counter {
	std::mutex mutex;
	long long count;
} Counter;

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
		"\"\r\nContent-Type:text-plain; charset=ISO-8859-1\r\nContent-Transfer-Encoding: 8bit\r\n\r\n";
	body.insert(body.end(), textPartStart.begin(), textPartStart.end());

	std::string textData = text;
	body.insert(body.end(), textData.begin(), textData.end());

	std::string textPartEnd = "\r\n--" + boundary + "\r\n";
	body.insert(body.end(), textPartEnd.begin(), textPartEnd.end());

	return;
}

void AddBinaryPart(std::vector<char>& body, unsigned char* buffer, unsigned int lengh, std::string& boundary, std::string fileName)
{
	// Add picture
	std::string partStart = "Content-Disposition: form-data; name=\"files\"; filename=\"" + \
		fileName + "\"\r\nContent-Type: multipart/form-data; charset=ISO-8859-1\r\nContent-Transfer-Encoding: binary\r\n\r\n";
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
	httplib::Client cli("10.142.193.10", HTTP_POST_PORT);

	std::string path;
	httplib::Headers headers;
	std::string body;
	std::string contentType;

	if (msg.type == MSG_TYPE_STOP) {
		path = "/inspection/stopFlag";
		//headers = { {"Content-Type", "application/json"} };
		contentType = "application/json";
		nlohmann::json jsonObject;
		jsonObject["pipelineCode"] = msg.pipelineCode;
		jsonObject["productSn"] = msg.productSn;
		body = jsonObject.dump();
	}
	else {
		path = "/inspection/upload";
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
				AddBinaryPart(body1, it->imageBuffer, it->imageLen, boundary, imageName);
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
			auto sound = readPCMFile(soundPath + soundName);
			// Add sound
			AddBinaryPart(body1, (unsigned char*)sound.data(), sound.size(), boundary, soundName);
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
	log_info("End post http msg, msgId: " + std::to_string(msg.msgId));

	if (res && res->status == 200) {
		log_info("Http msg post successed! msgId: " + std::to_string(msg.msgId));
	}
	else {
		log_error("Http msg post failed! msgId:" + std::to_string(msg.msgId));
	}

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
        deviceGPIO->SetPinLevel(6, 1);
        std::this_thread::sleep_for(std::chrono::seconds(3));
        deviceGPIO->SetPinLevel(6, 0);

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
	HANDLE hClearLog = CreateThread(NULL, 0, CheckAndClearLog, NULL, 0, NULL);
	log_init("AIQIForHaier","D:/AIQIforHaier/logs/rotating.txt", 1048576 * 50, 3);

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
	StartSelfTesting();
	GetConfig();
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
			PrintDevices();
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

DWORD __stdcall CheckAndClearLog(LPVOID lpParam) {
	const int max_chars = 6000;

	while (true) {
		//Get the length of the text in edit control
		int textLength = GetWindowTextLength(hEdit);

		if (textLength > max_chars) {
			SetWindowText(hEdit, TEXT(""));
		}

		std::this_thread::sleep_for(std::chrono::seconds(5));
	}

	return 0;
}

// 各个按钮对应的函数
void StartSelfTesting(/*HWND hWnd*/) {
	HWND hWnd = FindWindow(NULL, szTitle);
	HMENU hMenu = GetMenu(hWnd);
	if (f_SELFTESTING == false) {
		CheckMenuItem(hMenu, ID_SELFTESTING, MF_CHECKED); // 打勾
		f_SELFTESTING = true;
		AppendLog(_T("开始自检\n"));
		if (!DeviceConfigued) {
			AppendLog(_T("开始设备基础信息读取和初始化\n"));
			DeviceConfigued = true;

			std::string deviceConfigPath = projDir.c_str();
			deviceConfigPath.append("\\basicdeviceconfig\\deviceconfiglist.json");
			std::ifstream jsonFile(deviceConfigPath); // todo: 文件路径可配置
			if (!jsonFile.is_open()) {
				AppendLog(_T("Json File is not opened!\n"));
				return;
			}
			jsonFile.seekg(0); // 回到起始位
			nlohmann::json jsonObj;
			try {
				jsonFile >> jsonObj;
				AppendLog(_T("Open Json File success!\n"));
				nlohmann::json deviceConfigList = jsonObj["deviceConfigList"];
				for (const auto& deviceConfig : deviceConfigList) {
					//AppendLog(StringToWstring(deviceConfig.dump()).c_str());
					//AppendLog(_T("\n"));
					std::string sdeviceTypeId, sdeviceTypeName, sdeviceTypeCode, sdeviceCode, sdeviceName;
					if (deviceConfig.contains("deviceTypeId")) {
						sdeviceTypeId = deviceConfig["deviceTypeId"];
					}
					if (deviceConfig.contains("deviceTypeName")) {
						sdeviceTypeName = deviceConfig["deviceTypeName"];
					}
					if (deviceConfig.contains("deviceTypeCode")) {
						sdeviceTypeCode = deviceConfig["deviceTypeCode"];
					}
					if (deviceConfig.contains("deviceCode")) {
						sdeviceCode = deviceConfig["deviceCode"];
					}
					if (deviceConfig.contains("deviceName")) {
						sdeviceName = deviceConfig["deviceName"];
					}
					if (deviceConfig.contains("deviceTypeCode")) {
						switch (deviceTypeCodemap[deviceConfig["deviceTypeCode"]]) {
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

							AppendLog(_T("初始化GPIO设备信息完成！\n"));
						}
							  break;
							  //peSwitch(int gpioPin,UINT BaseMsg, std::string l_codereaderID, BOOLEAN istrigger, GPIO* gpio, HWND whandle,
							  //std::string deviceTypeId, std::string deviceTypeName, std::string deviceTypeCode, std::string deviceCode, std::string deviceName) :
							  //equnit(deviceTypeId, deviceTypeName, deviceTypeCode, deviceCode, deviceName),
							  //    pin(gpioPin), peBaseMsg(BaseMsg), codereaderID(l_codereaderID), b_istrigger(istrigger), gpUnit(gpio), hwnd(whandle)
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

										//triggerMap.insert(std::make_pair(pin, linkedScanningGun));

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
										AppendLog(std::to_wstring(pin).c_str());
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
										AppendLog(_T("警告：初始化设备信息存在未知参数！\n"));
										break;
									}
								}
							}

							std::string codereaderID = "";
							peSwitch* lSwitch = new peSwitch(pin, WM_GPIOBASEMSG, codereaderID, true, deviceGPIO, hWnd, sdeviceTypeId, sdeviceTypeName, sdeviceTypeCode, sdeviceCode, sdeviceName);
							deviceMap.insert(std::make_pair(sdeviceCode, lSwitch));
							AppendLog(_T("初始化光电开关信息完成！\n"));
						}
							  break;
						case 2: { //摄像机
							std::string ipAddr = deviceConfig["ipAddr"];
							Camera* cdevice = new Camera(ipAddr, sdeviceTypeId, sdeviceTypeName, sdeviceTypeCode, sdeviceCode, sdeviceName);
							if (deviceConfig.contains("deviceParamConfigList")) {
								cdevice->SetValuesByJson(deviceConfig["deviceParamConfigList"]);
							}
							deviceMap.insert(std::make_pair(sdeviceCode, cdevice));
							AppendLog(_T("初始化相机信息完成！\n"));
							break;
						}
						case 3: { //扫码枪
							std::string ipAddr = deviceConfig["ipAddr"];
							CodeReader* cddevice = new CodeReader(ipAddr, sdeviceTypeId, sdeviceTypeName, sdeviceTypeCode, sdeviceCode, sdeviceName);
							if (deviceConfig.contains("deviceParamConfigList")) {
								cddevice->SetValuesByJson(deviceConfig["deviceParamConfigList"]);
							}
							deviceMap.insert(std::make_pair(sdeviceCode, cddevice));
							AppendLog(_T("初始化读码器信息完成！\n"));
							break;
						}
						case 4: { //音频设备
							AudioEquipment* audioDevice = new AudioEquipment(sdeviceTypeId, sdeviceTypeName, sdeviceTypeCode, sdeviceCode, sdeviceName);
							deviceMap.insert(std::make_pair(sdeviceCode, audioDevice));
							AppendLog(_T("初始化音频设备信息完成！\n"));
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
										AppendLog(_T("警告：初始化设备信息存在未知参数！\n"));
										break;
									}
								}

							}
							SerialCommunication* scDevice = new SerialCommunication(sdeviceTypeId, sdeviceTypeName, sdeviceTypeCode, sdeviceCode, sdeviceName, portName, baudRate, message, devicePin);
							deviceMap.insert(std::make_pair(sdeviceCode, scDevice));
							AppendLog(_T("初始化遥控器信息完成！\n"));
							break;
						}
						case 6: { //音频设备
							AudioEquipment* audioDevice = new AudioEquipment(sdeviceTypeId, sdeviceTypeName, sdeviceTypeCode, sdeviceCode, sdeviceName);
							deviceMap.insert(std::make_pair(sdeviceCode, audioDevice));
							AppendLog(_T("初始化音频设备信息完成！\n"));
							break;
						}
						default:
							AppendLog(_T("警告：初始化设备信息存在未知设备！\n"));
							break;
						}
					}
				}
			}
			catch (const nlohmann::json::parse_error& e) {
				AppendLog(StringToLPCWSTR(e.what()));
				f_SELFTESTING = false;
				CheckMenuItem(hMenu, ID_SELFTESTING, MF_UNCHECKED);
				return;
			}
			DeviceConfigued = true;
		}
		else {
			AppendLog(_T("检测线设备配置已获取完毕！"));
		}

		// 开始自检，启动设备；自检过程只打开GPIO，不会涉及peSwitch
		// todo: 优先初始化GPIO
		AppendLog(_T("开始自检\n"));
		int testflag = 0;
		for (auto it = deviceMap.begin(); it != deviceMap.end(); ++it) {
			switch (deviceTypeCodemap[it->second->e_deviceTypeCode]) {
			case 0: {//GPIO
				GPIO* deviceGPIO = dynamic_cast<GPIO*>(it->second);
				if (deviceGPIO->Init() && deviceGPIO->StartThread()) {
					AppendLog(_T("GPIO 自检通过 \n"));
					testflag++;
				}
				else {
					AppendLog(_T("GPIO 自检失败 \n"));
				}
				break;
			}
			case 1://光电开关跳过
				AppendLog(_T("光电开关 自检跳过 \n"));
				testflag++;
				break;
			case 2: {//摄像机开关
				Camera* deviceCamera = dynamic_cast<Camera*>(it->second);
				if (deviceCamera->GetCameraByIpAddress() && deviceCamera->Init()) {
					deviceCamera->StartGrabbing();
					testflag++;
				}
				else {
					std::string logStr = "Camera " + it->first + " selftest error!\n";
					AppendLog(StringToLPCWSTR(logStr));
				}
				break;
			}
			case 3: {//码枪
				CodeReader* deviceCodeReader = dynamic_cast<CodeReader*>(it->second);
				if (deviceCodeReader->GetCodeReaderByIpAddress() && deviceCodeReader->Init()) {
					deviceCodeReader->StartGrabbing();
					testflag++;
				}
				else {
					std::string logStr = "CodeReader " + it->first + " selftest error!\n";
					AppendLog(StringToLPCWSTR(logStr));
				}
				break;
			}
			case 4: { // 音频
				AudioEquipment* audioDevice = dynamic_cast<AudioEquipment*>(it->second);
				
				std::string soundFilePath = projDir.c_str();
				soundFilePath.append("\\sounds\\");
				audioDevice->ReadFile(soundFilePath);

				//int ret = audioDevice->Init(soundFile); // todo: 解决未找到音频设备时抛出异常的问题

				testflag++; // todo: 加条件
				break;
			}
			case 5: {
				// 红外发射器
				SerialCommunication* scDevice = dynamic_cast<SerialCommunication*>(it->second);
				//if (scDevice->OpenSerial()) {
				testflag++;
				//}
				//else {
				//	std::string logStr = "Remote control " + it->first + " selftest error!\n";
				//	AppendLog(StringToLPCWSTR(logStr));
				//}
				break;
			}
			case 6: {
				AudioEquipment* audioDevice = dynamic_cast<AudioEquipment*>(it->second);
				testflag++;
				break;
			}
			default:
				break;
			}
		}
		if (testflag == deviceMap.size()) {
			AppendLog(_T("所有设备自检通过\n"));
		}
		else {
			std::string logStr = std::to_string(deviceMap.size() - testflag) + " devices self test error, other deivces success!s\n";
			AppendLog(StringToLPCWSTR(logStr));
		}
		CheckMenuItem(hMenu, ID_SELFTESTING, MF_UNCHECKED);
		f_SELFTESTING = false;
	}
	else {
		if (true == f_SELFTESTING) {
			CheckMenuItem(hMenu, ID_SELFTESTING, MF_UNCHECKED);
			f_SELFTESTING = false;
			AppendLog(_T("停止自检\n"));
		}
	}
	return;
}

void PrintDevices() {
	for (const auto& pair : deviceMap) {
		std::string logStr = pair.first + ": " + pair.second->e_deviceName + "\n";
		AppendLog(StringToLPCWSTR(logStr));
	}
}

// 处理得到 productMap
void GetConfig(/*HWND hWnd*/) {
	HWND hWnd = FindWindow(NULL, szTitle);
	HMENU hMenu = GetMenu(hWnd);
	if (f_GETCFG) {
		f_GETCFG = !f_GETCFG;
		CheckMenuItem(hMenu, ID_GETCFG, MF_UNCHECKED);
		return;
	}
	f_GETCFG = !f_GETCFG;
	CheckMenuItem(hMenu, ID_GETCFG, MF_CHECKED);

	// 调用 GetPipelineConfig.jar
	std::string jarPath = projDir.c_str();
	jarPath.append("\\GetPipelineConfig.jar");
	std::string cfgDir = projDir.c_str();
	cfgDir.append("\\productconfig\\");
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
			AppendLog(_T("等待配置文件超时，超时时间为20秒\n"));
			return;
		}
		Sleep(1000);
	}

	remove(flagpath.c_str());

	// 读取 pipelineConfig.json
	std::string configfile = projDir.c_str();
	configfile.append("\\productconfig\\pipelineConfig.json");
	std::ifstream jsonFile(configfile);
	if (!jsonFile.is_open()) {
		AppendLog(_T("Json File is not opened!\n"));
		CheckMenuItem(hMenu, ID_GETCFG, MF_UNCHECKED);
		return;
	}

	// 解析 json
	jsonFile.seekg(0);
	nlohmann::json jsonObj;
	try {
		jsonFile >> jsonObj;
		AppendLog(_T("Open Json File success!\n"));
		// 获取pipeline基本信息
		pipelineCode = jsonObj["pipelineCode"];
		pipelineName = jsonObj["pipelineName"];
		auto productConfigList = jsonObj["productConfigList"];
		for (auto productConfig : productConfigList) {
			// product
			std::string productName = productConfig["productName"];
			std::string productSnCode = productConfig["productSnCode"]; //前9个字符，码枪扫出的
			std::string productSnModel = productConfig["productSnModel"];
			std::string audioFileName = "xiaoyouxiaoyou";
			if (productConfig.contains("audioFileName"))
			{
				audioFileName = productConfig["audioFileName"];			
			}

			Product* tmpProduct = new Product(productSnCode, productName, productSnModel);

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
			productMap.insert(std::make_pair(productSnCode, tmpProduct));
		}
		//for (const auto& i : productMap) {
		//	std::string logStr = i.first + ": \n";
		//	auto tlm = i.second->testListMap;
		//	for (auto i = tlm->find(3)->second->nextunit; i != tlm->find(3)->second; i = i->nextunit) {
		//		logStr += i->deviceCode + ", ";
		//	}
		//	logStr += "\n";
		//	AppendLog(StringToLPCWSTR(logStr));
		//}
		CheckMenuItem(hMenu, ID_GETCFG, MF_UNCHECKED);
	}
	catch (const nlohmann::json::parse_error& e) {
		AppendLog(StringToLPCWSTR(e.what()));
		f_GETCFG = false;
		CheckMenuItem(hMenu, ID_GETCFG, MF_UNCHECKED);
		return;
	}
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
	ProcessUnit* procUnit;
	httpMsg msg;
};

struct CodeReaderPara {
	int gpioPin;
	bool result;
	string productSn;
};

DWORD __stdcall CodeReaderThread(LPVOID lpParam) {
	struct CodeReaderPara* param = static_cast<struct CodeReaderPara*>(lpParam);
	int gpioPin = param->gpioPin;

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

	param->result = true;
	param->productSn = productSn;

	return 0;
}

DWORD __stdcall MainWorkThread(LPVOID lpParam) {
	CloseHandle(GetCurrentThread());
	int gpioPin = (int)lpParam;
	std::vector<HANDLE> handles;

	struct CodeReaderPara param;
	param.gpioPin = gpioPin;
	param.result = false;
	HANDLE hCodeReader = CreateThread(NULL, 0, CodeReaderThread, &param, 0, NULL);
	handles.push_back(hCodeReader);

	std::string productSnCode = "XXXXXXXXX";

	// 跳过 pipeline 配置里不存在的商品总成号 productSnCode
	auto productItem = productMap.find(productSnCode);
	if (productItem == productMap.end()) {
		log_warn("Gpiopin " + std::to_string(gpioPin) + ": Product sn " + productSnCode + " does not exist!");
		return 0;
	}

	ProcessUnit* head = productItem->second->testListMap->find(gpioPin)->second;

	auto now = std::chrono::system_clock::now();
	auto duration_in_milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch());
	long start = duration_in_milliseconds.count();

	struct UnitWorkPara UnitParam[5];
	ProcessUnit* tmpFind = head->nextunit;
	int unitCount = 0;
	while (tmpFind != head) {
		auto now = std::chrono::system_clock::now();
		auto duration_in_milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch());
		long runla = duration_in_milliseconds.count();

		// 加句柄队列
		if ((runla - start) >= tmpFind->laterncy) {

			UnitParam[unitCount].procUnit = tmpFind;
			HANDLE hUnitWork = CreateThread(NULL, 0, UnitWorkThread, &UnitParam[unitCount], 0, NULL);
			handles.push_back(hUnitWork);
			tmpFind = tmpFind->nextunit;
			unitCount++;
		}
		else {
			Sleep(10);
		}
	}

	for (auto& handle : handles) {
		WaitForSingleObject(handle, INFINITE);
		CloseHandle(handle);
	}

	if (param.result == true) {
		for (int i = 0; i < unitCount; i++) {
			httpMsg& msg = UnitParam[i].msg;
			msg.productSn = param.productSn;
			msg.productSnCode = param.productSn.substr(0, 9);
			log_info("push msg, msgId: " + std::to_string(msg.msgId) + ", processSn: " + msg.productSn + ", processesTemplateCode : " + msg.processesTemplateCode);
			Singleton::instance().push(UnitParam[i].msg);
		}
		struct httpMsg msg;
		Counter.mutex.lock();
		Counter.count++;
		msg.msgId = Counter.count;
		Counter.mutex.unlock();
		msg.pipelineCode = pipelineCode;
		msg.productSn = param.productSn;
		msg.type = MSG_TYPE_STOP;

		Singleton::instance().push(msg);
		log_info("push stop msg, msgId: " + std::to_string(msg.msgId) + ", processSn: " + msg.productSn);
	}
	else {
		for (int i = 0; i < unitCount; i++) {
			for (int j = 0; j < UnitParam[i].msg.pictures.size(); j++) {
				delete[] UnitParam[i].msg.pictures[j].imageBuffer;
			}
		}
	}

	return 0;
}

//void TriggerOn(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
//	int gpioPin = static_cast<int>(lParam);
//	std::string triggerLog = "TriggerOn: wParam = " + std::to_string(static_cast<int>(wParam))
//		+ ", lParam = " + std::to_string(gpioPin) + "\n";
//	AppendLog(StringToLPCWSTR(triggerLog));

void TriggerOn(UINT gpioPin)
{
	std::string triggerLog = "TriggerOn: gpioPin = " + std::to_string(gpioPin) + "\n";
	AppendLog(StringToLPCWSTR(triggerLog));

	if (!f_QATESTING) {
		AppendLog(_T("f_QATESTING is false!\n"));//pin 脚触发
		return;
	}

	// 1. triggerMap 把引脚和扫码枪对应
	// 2. 根据扫码枪扫到的码，确定产品
	// 3. 根据产品确定执行的 process
	std::string loglockStr = std::to_string(__LINE__) + ", GPIO PIN " + std::to_string(gpioPin) + " before try lock\n";
	AppendLog(StringToLPCWSTR(loglockStr));
	if (!mtx[gpioPin].try_lock()) {
		loglockStr = std::to_string(__LINE__) + ", GPIO PIN " + std::to_string(gpioPin) + "  try lock fail\n";
		AppendLog(StringToLPCWSTR(loglockStr));
		return;
	}
	loglockStr = std::to_string(__LINE__) + ", GPIO PIN " + std::to_string(gpioPin) + "   locked \n";
	AppendLog(StringToLPCWSTR(loglockStr));
	if (isPinTriggered[gpioPin]) {
		std::string logStr = std::to_string(__LINE__) + ", GPIO PIN " + std::to_string(gpioPin) + " try trigger error\n";
		AppendLog(StringToLPCWSTR(logStr));
		mtx[gpioPin].unlock();
		return;
	}
	isPinTriggered[gpioPin] = true;

	HANDLE hdw = CreateThread(NULL, 0, MainWorkThread, (LPVOID)gpioPin, 0, NULL);
	return;
}

DWORD __stdcall UnitWorkThread(LPVOID lpParam) {
	//DWORD tid = GetCurrentThreadId();
	//std::string logStr = std::to_string(__LINE__) + ",tid " + std::to_string(tid) + " start!\n";
	//AppendLog(StringToLPCWSTR(logStr));
	struct UnitWorkPara *param = static_cast<struct UnitWorkPara *>(lpParam);
	ProcessUnit* unit = param->procUnit;

	std::string path = projDir.c_str();

	std::string logStr = "device type: " + unit->deviceTypeCode + " ,device code: " + unit->deviceCode + " called!\n";
	AppendLog(StringToLPCWSTR(logStr));

	switch (deviceTypeCodemap[unit->deviceTypeCode]) {
	case 2: { // Camera
		Camera* devicecm = dynamic_cast<Camera*>(unit->eq);
		log_info("Camera " + devicecm->e_deviceCode + " be called!");
		devicecm->Lock();
		devicecm->SetValuesByJson(unit->parameter);
//		devicecm->GetImage(path, unit);
		devicecm->GetImage(path, param);
		devicecm->UnLock();
		break;
	}
	case 3: { // ScanningGun
		CodeReader* deviceCR = dynamic_cast<CodeReader*>(unit->eq);
		std::vector<std::string> codeRes;
		log_info("Scan coder " + deviceCR->e_deviceCode + " be called!");

		deviceCR->Lock();
		deviceCR->SetValuesByJson(unit->parameter);
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
		msg.productSn = unit->productSn;
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
			msg.productSn = unit->productSn;
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

	//logStr = std::to_string(__LINE__) + ",tid " + std::to_string(tid) + " end!\n";
	//AppendLog(StringToLPCWSTR(logStr));
	return 0;
}

//void TriggerOff(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
//	int gpioPin = static_cast<int>(lParam);
//	std::string triggerLog = "TriggerOff: wParam = " + std::to_string(static_cast<int>(wParam))
//		+ ", lParam = " + std::to_string(gpioPin) + "\n";
//	AppendLog(StringToLPCWSTR(triggerLog));

void TriggerOff(UINT gpioPin)
{
    std::string triggerLog = "TriggerOff: gpioPin = " + std::to_string(gpioPin) + "\n";
    AppendLog(StringToLPCWSTR(triggerLog));

	if (!f_QATESTING) {
		AppendLog(StringToLPCWSTR(std::to_string(__LINE__) + "\n"));

		isPinTriggered[gpioPin] = false; // todo: check
		return;
	}

	AppendLog(StringToLPCWSTR(std::to_string(__LINE__) + "\n"));
	std::string loglockStr = std::to_string(__LINE__) + ", GPIO PIN " + std::to_string(gpioPin) + " before try lock\n";
	AppendLog(StringToLPCWSTR(loglockStr));
	if (mtx[gpioPin].try_lock()) {
		loglockStr = std::to_string(__LINE__) + ", GPIO PIN " + std::to_string(gpioPin) + "  try lock success unlock\n";
		AppendLog(StringToLPCWSTR(loglockStr));
		mtx[gpioPin].unlock();
		loglockStr = std::to_string(__LINE__) + ", GPIO PIN " + std::to_string(gpioPin) + "  unlock\n";
		AppendLog(StringToLPCWSTR(loglockStr));
		isPinTriggered[gpioPin] = false;
		return;
	}
	else {
		mtx[gpioPin].unlock();
		isPinTriggered[gpioPin] = false;
		loglockStr = std::to_string(__LINE__) + ", GPIO PIN " + std::to_string(gpioPin) + "  unlock\n";
		AppendLog(StringToLPCWSTR(loglockStr));
	}

	/*if (hMainWork[gpioPin] == nullptr) {
		AppendLog(StringToLPCWSTR(std::to_string(__LINE__) + "\n"));

		isPinTriggered[gpioPin] = false;
		return;
	}*/

	//AppendLog(StringToLPCWSTR(std::to_string(__LINE__) + "\n"));

	//HANDLE handle = hMainWork[gpioPin];
	//hMainWork[gpioPin] = nullptr;
	//allMainWorkHandles.remove(handle);

	//std::string logString = std::to_string(__LINE__) + ", trigger off, handle count: " + std::to_string(allMainWorkHandles.size()) + "\n";
	//AppendLog(StringToLPCWSTR(logString));


	//BOOLEAN bflage = false;
	//while (false== bflage) {
	//	std::string loglockStr = std::to_string(__LINE__) + ", GPIO PIN " + std::to_string(gpioPin) + " before try lock\n";
	//	AppendLog(StringToLPCWSTR(loglockStr));
	//	if (mtx[gpioPin].try_lock()) {
	//		loglockStr = std::to_string(__LINE__) + ", GPIO PIN " + std::to_string(gpioPin) + "   locked \n";
	//		AppendLog(StringToLPCWSTR(loglockStr));
	//		WaitForSingleObject(handle, INFINITE);
	//		CloseHandle(handle);
	//		mtx[gpioPin].unlock();
	//		bflage = true;
	//		loglockStr = std::to_string(__LINE__) + ", GPIO PIN " + std::to_string(gpioPin) + "   unlocked \n";
	//		AppendLog(StringToLPCWSTR(loglockStr));
	//	}
	//	else {
	//		Sleep(50);
	//	}
	//	
	//}
	//loglockStr = std::to_string(__LINE__) + ", GPIO PIN " + std::to_string(gpioPin) + "   locked \n";
	//AppendLog(StringToLPCWSTR(loglockStr));

	//AppendLog(StringToLPCWSTR(std::to_string(__LINE__) + "\n"));

	//isPinTriggered[gpioPin] = false;

	//AppendLog(StringToLPCWSTR(std::to_string(__LINE__) + "\n")); 

	/*auto it = map2bTest.find(gpioPin);
	if (it == map2bTest.end()) {
		AppendLog(L"map2bTest find error\n");
		isPinTriggered[gpioPin] = false;
		return;
	}

	AppendLog(StringToLPCWSTR(std::to_string(__LINE__) + "\n"));

	product2btest* p2t = it->second;
	if (p2t == nullptr) {
		AppendLog(L"map2bTest[gpioPin] error\n");
		isPinTriggered[gpioPin] = false;
		return;
	}

	AppendLog(StringToLPCWSTR(std::to_string(__LINE__) + "\n"));

	ProcessUnit* processUnitListHead = p2t->processUnitListHead;
	p2t->processUnitListHead = nullptr;
	isPinTriggered[gpioPin] = false;

	AppendLog(StringToLPCWSTR(std::to_string(__LINE__) + "\n"));*/

	//SetPostFlag(processUnitListHead);
}

void SetPostFlag(ProcessUnit* processUnitListHead) {
	ProcessUnit* curProcess = processUnitListHead->nextunit;
	while (curProcess != nullptr && curProcess != processUnitListHead) {
		std::string processPath = "D:\\AIQIforHaier";
		//std::string tmpProductionSnModel = curProcess->productSnModel.replace(curProcess->productSnModel.begin(), curProcess->productSnModel.end(), "/", "_");
		processPath += "\\" + pipelineCode + "\\" + curProcess->productSnModel + "\\" + curProcess->productSn + "\\" + curProcess->processesCode;
		//processPath += "\\" + pipelineCode + "\\" + tmpProductionSnModel + "\\" + curProcess->productSn + "\\" + curProcess->processesCode;

		std::string logPath = "D:\\AIQIforHaier";
		logPath += "\\PostFiles.log";

		//std::string command = "start /b java -jar " + jarPath + " " + baseUrl + " " + processPath + " requestArgs.json " + logPath;
		//std::system(command.c_str());

		auto now = std::chrono::high_resolution_clock::now();
		auto duration = now.time_since_epoch();
		auto microseconds = std::chrono::duration_cast<std::chrono::microseconds>(duration).count();

		// 创建一个json对象
		nlohmann::json reqJson;
		reqJson["baseUrl"] = baseUrl;
		reqJson["processPath"] = processPath;
		reqJson["requestArgsFileName"] = "requestArgs.json";
		reqJson["logPath"] = logPath;

		// req json
		std::string reqPath = "D:\\AIQIforHaier";
		reqPath += "\\post\\req\\" + std::to_string(microseconds) + ".json";
		std::ofstream o1(reqPath);
		o1 << reqJson.dump(4) << std::endl;
		o1.close();

		// blank flag
		std::string flagPath = "D:\\AIQIforHaier";
		flagPath += "\\post\\flag\\" + std::to_string(microseconds);
		std::ofstream o2(flagPath);
		o2.close();

		curProcess = curProcess->nextunit;
	}
}