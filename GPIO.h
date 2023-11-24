#pragma once

#include "libIoCtrl/libIoCtrl.h"
#include <iostream>
#include "equnit.h"
#include <windows.h>
#include <unordered_map>
#include <thread>
#include <atomic>
#include <chrono>

struct messagepair {
	UINT peMsgon;
	UINT peMsgoff;
	HWND hwnd;
};

class GPIO : public equnit {
private:
	void* m_handle = nullptr;
	char* m_bios_id = (char*)"C58.C57-H2.XX";
	int m_number = 8;

	HWND hwnd = nullptr;
	std::unordered_map<int, HWND> msgmap;
	UINT GPIOBASEMSG;

	HANDLE thread_handle = nullptr;
	BOOLEAN is_thread_running = false;

	static DWORD WINAPI MainWorkThread(LPVOID lpParam);
public:
	GPIO(std::string deviceCode) { e_deviceCode = deviceCode; };
	GPIO(std::string deviceTypeId, std::string deviceTypeName, std::string deviceTypeCode,
		std::string deviceCode, std::string deviceName, HWND wInstnd, char* bios_id, int p_number, UINT BASEMSG) :
		equnit(deviceTypeId, deviceTypeName, deviceTypeCode, deviceCode, deviceName), hwnd(wInstnd), m_bios_id(bios_id), m_number(p_number), GPIOBASEMSG(BASEMSG) {}
	~GPIO() { StopThread(); }
	bool Init();
	bool Deinit();
	bool SetGpioParam(std::string deviceTypeId, std::string deviceTypeName, std::string deviceTypeCode,
		std::string deviceName, HWND wInstnd, char* bios_id, int p_number, UINT BASEMSG);
	size_t RegistDevice(int pin, HWND hwnd);
	void SetHwnd(HWND hwnd) { this->hwnd = hwnd; }
	int Read(const int pinNumber, unsigned char& curState);
	int ReadMultipleTimes(const int pinNumber, unsigned char& curState, const int times = 10);

	bool StartThread();
	void StopThread();
};
