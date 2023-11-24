// AIQIforHaier.cpp : 定义应用程序的入口点。

#define _CRT_SECURE_NO_WARNINGS

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
#include "MessageQueue.h"
#pragma comment(lib, "ws2_32.lib")

#define MAX_LOADSTRING 100
#define HTTP_POST_PORT 10001

std::mutex mtx[8];
bool isPinTriggered[8]; // GPIO 针脚是否触发状态
HANDLE hMainWork[8]; // 当前线程句柄
std::list<HANDLE> allMainWorkHandles; // 所有线程句柄
int remoteCtrlPin;

std::string baseUrl = "http://192.168.0.189:10001";
std::string pipelineCode = "CX202309141454000002"; // 一台工控机只跑一个 pipeline
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
DWORD __stdcall SerialCommunicationThread(LPVOID lpParam);
DWORD __stdcall MainWorkThread(LPVOID lpParam);
DWORD __stdcall UnitWorkThread(LPVOID lpParam);

void AddTextPart(std::vector<char> &body, std::string &text, std::string &boundary, std::string name)
{
	std::string textPartStart = "Content-Disposition: form-data; name=\""+name+"\"\r\nContent-Type:text-plain; charset=ISO-8859-1\r\nContent-Transfer-Encoding: 8bit\r\n\r\n";
	body.insert(body.end(), textPartStart.begin(), textPartStart.end());

	std::string textData = text;
	body.insert(body.end(), textData.begin(), textData.end());

	std::string textPartEnd = "\r\n--" + boundary + "\r\n";
	body.insert(body.end(), textPartEnd.begin(), textPartEnd.end());

	return;
}

void AddBinaryPart(std::vector<char>& body, unsigned char* imageBuffer, unsigned int imageLen, std::string& boundary, std::string imageName)
{
	// Add picture
	std::string partStart = "--" + boundary + "\r\nContent-Disposition: form-data; name=\"files\"; filename=\"" + imageName + "\"\r\nContent-Type: multipart/form-data; charset=ISO-8859-1\r\nContent-Transfer-Encoding: binary\r\n\r\n";
	body.insert(body.end(), partStart.begin(), partStart.end());

	std::vector<char> binaryData;
	for (int i = 0; i < imageLen; i++)
	{
		binaryData.push_back(imageBuffer[i]);
	}
	body.insert(body.end(), binaryData.begin(), binaryData.end());

	std::string partEnd = "\r\n--" + boundary + "\r\n";
	body.insert(body.end(), partEnd.begin(), partEnd.end());

	return;
}

void HttpPost(message &msg)
{
	HINTERNET hSession=NULL, hConnect=NULL, hRequest=NULL;
	BOOL  bResults = FALSE;
	DWORD dwSize = 0;
	DWORD dwDownloaded = 0;
	LPSTR pszOutBuffer;

	// Use WinHttpOpen to obtain a session handle.
	hSession = WinHttpOpen(L"WinHTTP Example/1.0",
		WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
		WINHTTP_NO_PROXY_NAME,
		WINHTTP_NO_PROXY_BYPASS, 0);

	// Specify an HTTP server.
	if (hSession)
		hConnect = WinHttpConnect(hSession, L"192.168.0.189",
			HTTP_POST_PORT, 0);

	// Create an HTTP request handle.
	if (hConnect)
		hRequest = WinHttpOpenRequest(hConnect, L"POST",
			L"/inspection/upload",
			NULL, WINHTTP_NO_REFERER,
			WINHTTP_DEFAULT_ACCEPT_TYPES,
			0);

	// Define boundary and headers
	std::string boundary = "----WebKitFormBoundary7MA4YWxkTrZu0gW";
	std::wstring headers = L"Content-Type:multipart/form-data;boundary=----WebKitFormBoundary7MA4YWxkTrZu0gW";

	auto now = std::chrono::system_clock::now();
	auto duration = now.time_since_epoch();
	auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
	std::string imageName = std::to_string(milliseconds) + ".jpeg";

    //Define body
	std::vector<char> body;

	if (msg.type == MSG_TYPE_PICTURE) {
		// Add picture
		AddBinaryPart(body, msg.imageBuffer, msg.imageLen, boundary, imageName);
	}
	else {
		// Add TEXT
		std::string partStart = "--" + boundary + "\r\n";
		body.insert(body.end(), partStart.begin(), partStart.end());
		AddTextPart(body, msg.text, boundary, "content");
	}

//	std::stringstream ss;
    std::string sampleTime = "123456789";
//	ss << msg.sampleTime;
//	ss >> sampleTime;

	// Add text part
	AddTextPart(body, msg.pipelineCode, boundary, "pipelineCode");
	AddTextPart(body, msg.processesCode, boundary, "processesCode");
	AddTextPart(body, msg.processesTemplateCode, boundary, "processesTemplateCode");
	AddTextPart(body, msg.productSn, boundary, "productSn");
	AddTextPart(body, msg.productSnCode, boundary, "productSnCode");
	AddTextPart(body, msg.productSnModel, boundary, "productSnModel");
	AddTextPart(body, sampleTime, boundary, "sampleTime");

	body.pop_back();
	body.pop_back();
	std::string End = "--\r\n";
	body.insert(body.end(), End.begin(), End.end());

	// Send a request.
	if (hRequest)
		bResults = WinHttpSendRequest(hRequest,
			headers.c_str(),
			headers.length(), static_cast<LPVOID>(body.data()),
			body.size(),
			body.size(), 0);

	// End the request.
	if (bResults)
		bResults = WinHttpReceiveResponse(hRequest, NULL);

	// Close any open handles.
	if (hRequest) WinHttpCloseHandle(hRequest);
	if (hConnect) WinHttpCloseHandle(hConnect);
	if (hSession) WinHttpCloseHandle(hSession);

	return;
}

DWORD HttpPostThread(LPVOID lpParam)
{
	struct message msg;

	while (true)
	{
		Singleton::instance().wait(msg);
		HttpPost(msg);

		if (msg.type == MSG_TYPE_PICTURE)
		{
			delete[] msg.imageBuffer;
		}
	}

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

	// 初始化全局字符串
	LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
	LoadStringW(hInstance, IDC_AIQIFORHAIER, szWindowClass, MAX_LOADSTRING);
	MyRegisterClass(hInstance);

	// 执行应用程序初始化:
	if (!InitInstance(hInstance, nCmdShow))
	{
		return FALSE;
	}

	HANDLE hHttpPost = CreateThread(NULL, 0, HttpPostThread, NULL, 0, NULL);

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
		TriggerOn(hWnd, message, wParam, lParam);
		break;
	case WM_GPIO_OFF:
		TriggerOff(hWnd, message, wParam, lParam);
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
				
				std::string soundFile = projDir.c_str();
				soundFile.append("\\sounds\\upset.pcm"); // todo: 根据配置设置播放的音频文件
				audioDevice->ReadFile(soundFile);

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
		if ((duration_now_in_seconds_now.count() - duration_in_seconds.count()) > 10) {
			AppendLog(_T("等待配置文件超时，超时时间为10秒\n"));
			return;
		}
		Sleep(1000);
	}

	// 读取 pipelineConfig.json
	//std::string configfile = ".\\productconfig\\pipelineConfig.json";
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

DWORD __stdcall TriggerOnXXX(LPVOID lpParam) {
	CloseHandle(GetCurrentThread());
	int gpioPin = (int)lpParam;
	std::string triggerLog = "*lpParam = " + std::to_string(gpioPin) + "\n";

	auto now1 = std::chrono::system_clock::now();
	auto timeMillis1 = std::chrono::duration_cast<std::chrono::milliseconds>(now1.time_since_epoch());
	long long timeMillisCount1 = timeMillis1.count();

	// 得到产品序列号前9位
	std::vector<std::string> vcodereaders = triggerMaps[gpioPin];
	std::vector<std::string> codereaderresults;
	for (std::string codereader : vcodereaders) {
		auto it = deviceMap.find(codereader);
		if (it == deviceMap.end()) {
			AppendLog(_T("光电开关绑定的扫码器未初始化，请检查配置！\n"));
			AppendLog(_T("光电开关故障\n"));
			//mtx[gpioPin].unlock();
			//return;
			return 0;
		}
		CodeReader* CR = dynamic_cast<CodeReader*>(it->second);
		std::string logStr = "CodeReader " + CR->e_deviceCode + " called!" + std::to_string(CR->GetAcquisitionBurstFrameCount()) + "frames\n";
		AppendLog(StringToLPCWSTR(logStr));
		CR->StartGrabbing();
		std::vector<std::string> results;
		int crRet = CR->ReadCode(results);
		CR->StopGrabbing();
		codereaderresults.insert(codereaderresults.end(), results.begin(), results.end());
		logStr = "CodeReader ret: " + std::to_string(crRet) + "\n";
		AppendLog(StringToLPCWSTR(logStr));
	}

	if (codereaderresults.empty()) {
		AppendLog(_T("扫码结果为空！\n"));
//		mtx[gpioPin].unlock();
		//return;
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
		AppendLog(_T("扫码错误，扫码结果不足13位！\n"));
		//return;
		return 0;
	}

	auto now2 = std::chrono::system_clock::now();
	auto timeMillis2 = std::chrono::duration_cast<std::chrono::milliseconds>(now2.time_since_epoch());
	long long timeMillisCount2 = timeMillis2.count();
	std::string Log = "Scancode time = " + std::to_string(timeMillisCount2 - timeMillisCount1) + "\n";
	AppendLog(StringToLPCWSTR(Log));

	//gpioPin = 3;
	//std::string productSn = "AABMZD00001E6PB1AVNP";
	std::string productSnCode = productSn.substr(0, 9);

	// 跳过 pipeline 配置里不存在的商品总成号 productSnCode
	auto productItem = productMap.find(productSnCode);
	if (productItem == productMap.end()) {
		std::string logStr = "Product " + productSnCode + " does not exist!\n";
		AppendLog(StringToLPCWSTR(logStr));
//		mtx[gpioPin].unlock();
		//return;
		return 0;
	}

	std::string logStr = "Product " + productSn + " scanned!\n";
	AppendLog(StringToLPCWSTR(logStr));

	ProcessUnit* head = productItem->second->testListMap->find(gpioPin)->second;
	auto now = std::chrono::system_clock::now();
	auto duration_in_milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch());
	long start = duration_in_milliseconds.count();
	//product2btest* p2t = new product2btest(gpioPin, productSn, head, start);

	auto it = map2bTest.find(gpioPin);
	if (it == map2bTest.end()) {
		AppendLog(L"map2bTest init error!");
//		mtx[gpioPin].unlock();
		//return;
		return 0;
	}
	it->second->pinNumber = gpioPin;
	it->second->productSn = productSn;
	it->second->productSnCode = productSnCode;
	it->second->processUnitListHead = head;
	//processUnitListFind(listHead->nextunit)
	//	lastTestTimestamp(timestamp)
	it->second->shotTimestamp = start;

	//map2bTest.insert(std::make_pair(gpioPin, p2t));

	//hMainWork[gpioPin] = CreateThread(NULL, 0, MainWorkThread, it->second, 0, NULL);
	HANDLE hdw = CreateThread(NULL, 0, MainWorkThread, it->second, 0, NULL);
	//allMainWorkHandles.push_back(hMainWork[gpioPin]);
	std::string logString = std::to_string(__LINE__) + ", trigger on, handle count: " + std::to_string(allMainWorkHandles.size()) + "\n";
	return 0;
}

void TriggerOn(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
	int gpioPin = static_cast<int>(lParam);
	std::string triggerLog = "TriggerOn: wParam = " + std::to_string(static_cast<int>(wParam))
		+ ", lParam = " + std::to_string(gpioPin) + "\n";

	AppendLog(StringToLPCWSTR(triggerLog));

	if (!f_QATESTING) {
		AppendLog(_T("f_QATESTING is false!\n"));//pin 脚触发
		return;
	}

	// 进行串口通信
	if (gpioPin == remoteCtrlPin) {
		//CreateThread(NULL, 0, SerialCommunicationThread, NULL, 0, NULL);
		
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

	HANDLE hdw = CreateThread(NULL, 0, TriggerOnXXX, (LPVOID)gpioPin, 0, NULL);
	return;

	//// 得到产品序列号前9位
	//auto it = deviceMap.find(triggerMap[gpioPin]);
	//if (it == deviceMap.end()) {
	//	AppendLog(_T("光电开关绑定的扫码器未初始化，请检查配置！\n"));
	//	return;
	//}

	//CodeReader* CR = dynamic_cast<CodeReader*>(it->second);
	//CR->StartGrabbing();
	//std::vector<std::string> results = CR->ReadCode();
	//CR->StopGrabbing();

	//std::string productSn = "";
	//for (size_t i = 0; i < results.size(); i++) {
	//	std::string tmpr = results[i];
	//	// 取最长
	//	if (tmpr.size() > productSn.size()) {
	//		productSn = tmpr;
	//	}
	//}
	//if (productSn.length() < 9) {
	//	AppendLog(_T("扫码错误，扫码结果不足9位！\n"));
	//	return;
	//}
	//std::string productSnCode = productSn.substr(0, 9);
/*
	auto now1 = std::chrono::system_clock::now();
	auto timeMillis1 = std::chrono::duration_cast<std::chrono::milliseconds>(now1.time_since_epoch());
	long long timeMillisCount1 = timeMillis1.count();

	// 得到产品序列号前9位
	std::vector<std::string> vcodereaders = triggerMaps[gpioPin];
	std::vector<std::string> codereaderresults;
	for (std::string codereader : vcodereaders) {
		auto it = deviceMap.find(codereader);
		if (it == deviceMap.end()) {
			AppendLog(_T("光电开关绑定的扫码器未初始化，请检查配置！\n"));
			AppendLog(_T("光电开关故障\n"));
			mtx[gpioPin].unlock();
			//return;
			return 0;
		}
		CodeReader* CR = dynamic_cast<CodeReader*>(it->second);
		std::string logStr = "CodeReader " + CR->e_deviceCode + " called!" + std::to_string(CR->GetAcquisitionBurstFrameCount()) +"frames\n";
		AppendLog(StringToLPCWSTR(logStr));
		CR->StartGrabbing();
		std::vector<std::string> results;
		int crRet = CR->ReadCode(results);
		CR->StopGrabbing();
		codereaderresults.insert(codereaderresults.end(), results.begin(), results.end());
		logStr = "CodeReader ret: " + std::to_string(crRet) + "\n";
		AppendLog(StringToLPCWSTR(logStr));
	}

	if (codereaderresults.empty()) {
		AppendLog(_T("扫码结果为空！\n"));
		mtx[gpioPin].unlock();
		//return;
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
		AppendLog(_T("扫码错误，扫码结果不足13位！\n"));
		//return;
		return 0;
	}

	auto now2 = std::chrono::system_clock::now();
	auto timeMillis2 = std::chrono::duration_cast<std::chrono::milliseconds>(now2.time_since_epoch());
	long long timeMillisCount2 = timeMillis2.count();
	std::string Log = "Scancode time = " + std::to_string(timeMillisCount2 - timeMillisCount1) + "\n";
	AppendLog(StringToLPCWSTR(Log));

	//gpioPin = 3;
	//std::string productSn = "AABMZD00001E6PB1AVNP";
	std::string productSnCode = productSn.substr(0, 9);

	// 跳过 pipeline 配置里不存在的商品总成号 productSnCode
	auto productItem = productMap.find(productSnCode);
	if (productItem == productMap.end()) {
		std::string logStr = "Product " + productSnCode + " does not exist!\n";
		AppendLog(StringToLPCWSTR(logStr));
		mtx[gpioPin].unlock();
		//return;
		return 0;
	}

	std::string logStr = "Product " + productSn + " scanned!\n";
	AppendLog(StringToLPCWSTR(logStr));

	ProcessUnit* head = productItem->second->testListMap->find(gpioPin)->second;
	auto now = std::chrono::system_clock::now();
	auto duration_in_milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch());
	long start = duration_in_milliseconds.count();
	//product2btest* p2t = new product2btest(gpioPin, productSn, head, start);

	auto it = map2bTest.find(gpioPin);
	if (it == map2bTest.end()) {
		AppendLog(L"map2bTest init error!");
		mtx[gpioPin].unlock();
		//return;
		return 0;
	}
	it->second->pinNumber = gpioPin;
	it->second->productSn = productSn;
	it->second->productSnCode = productSnCode;
	it->second->processUnitListHead = head;
	//processUnitListFind(listHead->nextunit)
	//	lastTestTimestamp(timestamp)
	it->second->shotTimestamp = start;

	//map2bTest.insert(std::make_pair(gpioPin, p2t));

	//hMainWork[gpioPin] = CreateThread(NULL, 0, MainWorkThread, it->second, 0, NULL);
	HANDLE hdw = CreateThread(NULL, 0, MainWorkThread, it->second, 0, NULL);
	//allMainWorkHandles.push_back(hMainWork[gpioPin]);
	std::string logString = std::to_string(__LINE__) + ", trigger on, handle count: " + std::to_string(allMainWorkHandles.size()) + "\n";
*/
	/*AppendLog(StringToLPCWSTR(logString));
	loglockStr = std::to_string(__LINE__) + ", GPIO PIN "  + " before wait";
	AppendLog(StringToLPCWSTR(loglockStr));
	if(0 != hdw){ WaitForSingleObject(hdw, INFINITE); }

	loglockStr = std::to_string(__LINE__) + ", GPIO PIN " + " after wait";
	AppendLog(StringToLPCWSTR(loglockStr));
	if (0 != hdw) { CloseHandle(hdw); }*/

	/*loglockStr = std::to_string(__LINE__) + ", GPIO PIN " + " after closehandle";
	AppendLog(StringToLPCWSTR(loglockStr));
	mtx[gpioPin].unlock();
	loglockStr = std::to_string(__LINE__) + ", GPIO PIN "  + "   unlocked \n";
	AppendLog(StringToLPCWSTR(loglockStr));*/
}

DWORD __stdcall SerialCommunicationThread(LPVOID lpParam) {
	//DWORD tid = GetCurrentThreadId();
	//std::string logStr = std::to_string(__LINE__) + ",tid " + std::to_string(tid) + " start!\n";
	//AppendLog(StringToLPCWSTR(logStr));

	CloseHandle(GetCurrentThread());

	//SerialCommunication* serialComm = nullptr;
	//for (auto it = deviceMap.begin(); it != deviceMap.end(); ++it) {
	//	if (it->second->e_deviceTypeCode == "RemoteControl") {
	//		serialComm = dynamic_cast<SerialCommunication*>(it->second);
	//	}
	//}
	//if (serialComm == nullptr) {
	//	AppendLog(L"RemoteControl not found!\n");
	//	return 0;
	//}

	//if (serialComm->SendMessage()) {
	//	return 1;
	//}

	Sleep(200);

	std::string pyPath = projDir.c_str();
	pyPath.append("\\SerialCommunication.py");
	std::string command = "python " + pyPath + " COM7 38400 FF1606160032A6EA0000C0200080800000000B7BB7004000000000F7A6EA0000C0200080000000000BFBB7004000000000F7A6EA0000C0200080800000000B7BB7004000000000F7A6EA0000C0200080000000000BFBB7004000000000F7A6EA0000C0200080800000000B7BB7004000000000F7A6EA0000C0200080000000000BFBB7004000000000F7AD84";
	command = "python " + pyPath + " COM7 38400 \"FF 16 06 16 00 32 A6 EA 00 00 C0 20 00 80 80 00 00 00 0B 7B B7 00 40 00 00 00 00 F7 A6 EA 00 00 C0 20 00 80 00 00 00 00 0B FB B7 00 40 00 00 00 00 F7 A6 EA 00 00 C0 20 00 80 80 00 00 00 0B 7B B7 00 40 00 00 00 00 F7 A6 EA 00 00 C0 20 00 80 00 00 00 00 0B FB B7 00 40 00 00 00 00 F7 A6 EA 00 00 C0 20 00 80 80 00 00 00 0B 7B B7 00 40 00 00 00 00 F7 A6 EA 00 00 C0 20 00 80 00 00 00 00 0B FB B7 00 40 00 00 00 00 F7 AD 84\"";
	system(command.c_str());

	return 0;
}

DWORD __stdcall MainWorkThread(LPVOID lpParam) { //每个pin绑定一个对应的工作线程和锁
	//DWORD tid = GetCurrentThreadId();
	//std::string logStr = std::to_string(__LINE__) + ",tid " + std::to_string(tid) + " start!\n";
	//AppendLog(StringToLPCWSTR(logStr));
	CloseHandle(GetCurrentThread());
	product2btest* myp2btest = static_cast<product2btest*>(lpParam);
	std::vector<HANDLE> handles;
	auto now = std::chrono::system_clock::now();
	auto duration_in_milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch());
	long start = duration_in_milliseconds.count();

	ProcessUnit* tmpFind = myp2btest->processUnitListHead->nextunit;
	int count = 0;
	int gpiopin = myp2btest->processUnitListHead->pin;
	while (tmpFind != myp2btest->processUnitListHead) {
		// for test
		//count++;
		//if (count > 20) {
		//	AppendLog(L"dead loop\n");
		//	break;
		//}

		auto now = std::chrono::system_clock::now();
		auto duration_in_milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch());
		long runla = duration_in_milliseconds.count();

		// 加句柄队列
		if ((runla - start) >= tmpFind->laterncy) {
			tmpFind->productSn = myp2btest->productSn;

/*			// 建立目录和配置文件
			std::string path = projDir.c_str();
			path += "\\" + pipelineCode + "\\" + tmpFind->productSnModel + "\\" + tmpFind->productSn + "\\" + tmpFind->processesCode;
			bool mkdirRet = CreateRecursiveDirectory(path);
			std::string logStr = "Directory " + path + " created ret: ";
			if (mkdirRet) {
				logStr += "true\n";
			}
			else {
				logStr += "false\n";
			}
			AppendLog(StringToLPCWSTR(logStr));

			nlohmann::json args;
			args["pipelineCode"] = pipelineCode;
			args["processesCode"] = tmpFind->processesCode;
			args["processesTemplateCode"] = tmpFind->processesTemplateCode;
			args["productSn"] = tmpFind->productSn;
			args["productSnCode"] = tmpFind->productSnCode;
			args["productSnModel"] = tmpFind->productSnModel;
			args["sampleTime"] = 111;
			std::ofstream file(path + "\\requestArgs.json");
			file << args;
*/
			HANDLE hUnitWork = CreateThread(NULL, 0, UnitWorkThread, tmpFind, 0, NULL);
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
	//ProcessUnit* processUnitListHead = myp2btest->processUnitListHead;
	//p2t->processUnitListHead = nullptr;
	//isPinTriggered[gpioPin] = false;

	//AppendLog(StringToLPCWSTR(std::to_string(__LINE__) + "\n"));

	//SetPostFlag(processUnitListHead);

	//logStr = std::to_string(__LINE__) + ",tid " + std::to_string(tid) + " end!\n";
	//AppendLog(StringToLPCWSTR(logStr));

	return 0;
}

DWORD __stdcall UnitWorkThread(LPVOID lpParam) {
	//DWORD tid = GetCurrentThreadId();
	//std::string logStr = std::to_string(__LINE__) + ",tid " + std::to_string(tid) + " start!\n";
	//AppendLog(StringToLPCWSTR(logStr));

	ProcessUnit* unit = static_cast<ProcessUnit*>(lpParam);

	std::string path = projDir.c_str();
	//add replace productionSnModel / to _
	//std::string tmpProductionSnModel = unit->productSnModel.replace(unit->productSnModel.begin(), unit->productSnModel.end(), "/", "_");

	path += "\\" + pipelineCode + "\\" + unit->productSnModel + "\\" + unit->productSn + "\\" + unit->processesCode;
		
	//path += "\\" + pipelineCode + "\\" + tmpProductionSnModel + "\\" + unit->productSn + "\\" + unit->processesCode;

	nlohmann::json args;
	args["pipelineCode"] = pipelineCode;
	args["processesCode"] = unit->processesCode;
	args["processesTemplateCode"] = unit->processesTemplateCode;
	args["productSn"] = unit->productSn;
	args["productSnCode"] = unit->productSnCode;
	args["productSnModel"] = unit->productSnModel;
	args["sampleTime"] = 111;

	std::string logStr = "device type:" + unit->deviceTypeCode + ", device code:" + unit->deviceCode + " called!\n";
	AppendLog(StringToLPCWSTR(logStr));

	switch (deviceTypeCodemap[unit->deviceTypeCode]) {
	case 2: { // Camera
		Camera* devicecm = dynamic_cast<Camera*>(unit->eq);
		devicecm->Lock();
		devicecm->SetValuesByJson(unit->parameter);
		devicecm->StartGrabbing();
		devicecm->GetImage(path, unit);
		devicecm->StopGrabbing();
		devicecm->UnLock();
		break;
	}
	case 3: { // ScanningGun
		CodeReader* deviceCR = dynamic_cast<CodeReader*>(unit->eq);
		deviceCR->SetValuesByJson(unit->parameter);
		deviceCR->StartGrabbing();
		std::vector<std::string> codeRes;
		int crRet = deviceCR->ReadCode(codeRes);
		deviceCR->StopGrabbing();
		std::string logStr = "device code:" + unit->deviceCode + ", called ret:"+ std::to_string(crRet) + ", code count: " + std::to_string(codeRes.size()) + "\n";
		AppendLog(StringToLPCWSTR(logStr));
//		args["content"] = codeRes;
//		std::ofstream file(path + "\\requestArgs.json");
//		file << args.dump(4) << std::endl;
//		file.close();

		struct message msg;
		msg.pipelineCode = pipelineCode;
		msg.processesCode = unit->processesCode;
		msg.processesTemplateCode = unit->processesTemplateCode;
		msg.productSn = unit->productSn;
		msg.productSnCode = unit->productSnCode;
		msg.productSnModel = unit->productSnModel;
		msg.type = MSG_TYPE_TEXT;
		if (!codeRes.empty()) {
			msg.text = codeRes[0];
		}
		else {
			msg.text = "";
		}
		Singleton::instance().push(msg);

		break;
	}
	case 4: { // Speaker
		AppendLog(L"Speaker triggers.\n");
		AudioEquipment* audioDevice = dynamic_cast<AudioEquipment*>(unit->eq);

		// 初始化
		int ret = audioDevice->Init(); // todo: 解决未找到音频设备时抛出异常的问题

		// 录制 todo: 考虑录制时间
		std::string recordFile = path + "\\audio_data.raw";
		std::string logStr = "Record: ";
		logStr.append(recordFile).append("\n");
		AppendLog(StringToLPCWSTR(logStr));

		int audioRet = audioDevice->PlayAndRecord(recordFile, 5);
		logStr = "Ret: ";
		logStr.append(std::to_string(audioRet)).append("\n");
		AppendLog(StringToLPCWSTR(logStr));

		// 音频文件处理
		std::string leftFile = path + "\\left.pcm";
		std::string rightFile = path + "\\right.pcm";
		logStr = "leftFile: ";
		logStr.append(leftFile).append("\n");
		AppendLog(StringToLPCWSTR(logStr));
		logStr = "rightFile: ";
		logStr.append(rightFile).append("\n");
		AppendLog(StringToLPCWSTR(logStr));

		audioDevice->SeparateStereoChannels(recordFile, leftFile, rightFile);
		audioDevice->To16k(leftFile);
		audioDevice->To16k(rightFile);

		// 删除文件
		deleteFile(recordFile);
		deleteFile(leftFile);
		deleteFile(rightFile);
		AppendLog(L"3 Files deleted.\n");

		audioDevice->Terminate();

		break;
	}
	case 5: { // RemoteControl
		// todo: 加串口
		break;
	}
	default:
		break;
	}

	//logStr = std::to_string(__LINE__) + ",tid " + std::to_string(tid) + " end!\n";
	//AppendLog(StringToLPCWSTR(logStr));
	return 0;
}

void TriggerOff(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
	int gpioPin = static_cast<int>(lParam);
	std::string triggerLog = "TriggerOff: wParam = " + std::to_string(static_cast<int>(wParam))
		+ ", lParam = " + std::to_string(gpioPin) + "\n";
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
		std::string processPath = projDir.c_str();
		//std::string tmpProductionSnModel = curProcess->productSnModel.replace(curProcess->productSnModel.begin(), curProcess->productSnModel.end(), "/", "_");
		processPath += "\\" + pipelineCode + "\\" + curProcess->productSnModel + "\\" + curProcess->productSn + "\\" + curProcess->processesCode;
		//processPath += "\\" + pipelineCode + "\\" + tmpProductionSnModel + "\\" + curProcess->productSn + "\\" + curProcess->processesCode;

		std::string logPath = projDir.c_str();
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
		std::string reqPath = projDir.c_str();
		reqPath += "\\post\\req\\" + std::to_string(microseconds) + ".json";
		std::ofstream o1(reqPath);
		o1 << reqJson.dump(4) << std::endl;
		o1.close();

		// blank flag
		std::string flagPath = projDir.c_str();
		flagPath += "\\post\\flag\\" + std::to_string(microseconds);
		std::ofstream o2(flagPath);
		o2.close();

		curProcess = curProcess->nextunit;
	}
}