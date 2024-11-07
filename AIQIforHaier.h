#pragma once

#include "resource.h"
#include <thread>
#include <mutex>
#include <fstream>
#include "product2btest.h"
#include "equnit.h"
#include "Camera.h"
#include "CodeReader.h"
#include "AudioEquipment.h"
#include "SerialCommunication.h"
#include <filesystem>
#include <iostream>

std::string projDir = ".";
std::unordered_map<int, std::vector<std::string>> triggerMaps;
//std::unordered_map<int, std::string> triggerMap;       // GPIO pin - 扫描枪deviceCode
std::unordered_map<std::string, equnit*> deviceMap;    // deviceCode - 设备
std::shared_ptr<std::unordered_map<std::string, std::shared_ptr<Product>>> productMap;  // productSnCode - Product
std::unordered_map<int, product2btest*> map2bTest;     // GPIO pin - product2btest

bool f_SELFTESTING = false;
bool f_QATESTING = false;
bool f_GETCFG = false;
bool DeviceConfigued = false;

std::shared_ptr<std::unordered_map<std::string, std::shared_ptr<Product>>> ParsePipelineConfig(nlohmann::json& jsonObj);
nlohmann::json ReadPipelineConfig();
bool FetchPipeLineConfigFile();
bool ParseBasicConfig();
bool StartDeviceSelfTesting();

//void TriggerOn(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
//void TriggerOff(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

void TriggerOn(UINT gpioPin);
void TriggerOff(UINT gpioPin);


// before c++17
//std::wstring StringToWstring(const std::string& s) {
//	std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t> converter;
//	return converter.from_bytes(s);
//}

// since c++17
std::wstring StringToWstring(const std::string& s) {
	std::wstring ws(s.size(), L' '); // Overestimate number of code points.
	ws.resize(mbstowcs(&ws[0], s.c_str(), s.size())); // Shrink to fit.
	return ws;
}

std::wstring StringToWstring2(const std::string& s) {
	int size = MultiByteToWideChar(CP_UTF8, 0, &s[0], -1, NULL, 0);
	std::wstring wstr(size - 1, 0);
	MultiByteToWideChar(CP_UTF8, 0, &s[0], -1, &wstr[0], size);
	return wstr;
}

LPCWSTR StringToLPCWSTR(const std::string& s) {
	int size = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, NULL, 0);
	wchar_t* wstr = new wchar_t[size];
	MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, wstr, size);
	return wstr;
}

bool IsDirectory(const char* path) {
	DWORD attributes = GetFileAttributesA(path);
	if (attributes == INVALID_FILE_ATTRIBUTES) {
		// 错误处理
		return false;
	}
	return (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

bool CreateRecursiveDirectory(const std::string& path) {
	std::string dir = path;
	std::replace(dir.begin(), dir.end(), '/', '\\');

	if (dir[dir.size() - 1] != '\\') {
		dir += '\\';
	}
	int pos = 0;
	for (pos = dir.find_first_of('\\', pos + 3); pos != -1; pos = dir.find_first_of('\\', pos + 1)) {
		std::string subDir = dir.substr(0, pos);

		if (subDir.size() == 0) {
			return false;
		}

		if (!CreateDirectoryA(subDir.c_str(), NULL) && GetLastError() != ERROR_ALREADY_EXISTS) {
			return false;
		}
	}

	return IsDirectory(dir.c_str());
}

void deleteFile(const std::filesystem::path& filename) {
	try {
		if (std::filesystem::remove(filename)) {
			std::cout << "文件删除成功！\n";
		}
		else {
			std::cout << "文件不存在！\n";
		}
	}
	catch (std::filesystem::filesystem_error& e) {
		std::cout << "发生错误：" << e.what() << '\n';
	}
}
