#include "GPIO.h"
#include <chrono>
#include <thread>
#include <vector>
#include <string>

bool GPIO::Init() {
	int ret = libIoCtrlInit(&m_handle, m_bios_id);
	if (ret != 0) {
		std::cerr << "Init lib failed: " << ret << std::endl;
		return false;
	}
	return true;
}

bool GPIO::Deinit() {
	if (m_handle == nullptr) {
		return true;
	}
	int ret = libIoCtrlDeinit(&m_handle);
	if (ret != 0) {
		std::cerr << "Deinit lib failed: " << ret << std::endl;
		return false;
	}
	return true;
}

size_t GPIO::RegistDevice(int pin, HWND hwnd) {
	msgmap.insert(std::make_pair(pin, hwnd));
	return msgmap.size();
}

int GPIO::Read(int pinNumber, unsigned char& curState) {
	int ret = getPinLevel(&m_handle, (GPIO_PIN_INDEX)pinNumber, &curState);
	if (ret != 0) {
		std::cerr << "getPinLevel failed: " << ret << std::endl;
		return ret;
	}
	std::cout << " GPIO_PIN_INDEX = " << pinNumber << " , " << " curState = " << (int)curState << std::endl;
	return ret;
}

int GPIO::ReadMultipleTimes(const int pinNumber, unsigned char& curState, const int times) {
	unsigned char state = 2;
	int ret = getPinLevel(&m_handle, (GPIO_PIN_INDEX)pinNumber, &state);
	if (ret != 0) {
		std::cerr << "getPinLevel failed: " << ret << std::endl;
		return ret;
	}

	for (int i = 0; i < times - 1; ++i) {
		unsigned char tempState = 2;
		int ret = getPinLevel(&m_handle, (GPIO_PIN_INDEX)pinNumber, &tempState);
		if (ret != 0) {
			std::cerr << "getPinLevel failed: " << ret << std::endl;
			return ret;
		}
		if (tempState != state) {
			std::cerr << "Level transition!" << std::endl;
			return -1;
		}
	}

	curState = state;
	return 0;
}

bool GPIO::SetGpioParam(std::string deviceTypeId, std::string deviceTypeName, std::string deviceTypeCode,
	std::string deviceName, HWND wInstnd, char* bios_id, int p_number, UINT BASEMSG) {
	//equnit(deviceTypeId, deviceTypeName, deviceTypeCode, deviceCode, deviceName) , hwnd(wInstnd), m_bios_id(bios_id), m_number(p_number), GPIOBASEMSG(BASEMSG
	e_deviceCode = deviceName;
	e_deviceTypeCode = deviceTypeCode;
	e_deviceTypeId = deviceTypeId;
	e_deviceTypeName = deviceTypeName;
	hwnd = wInstnd;
	m_bios_id = bios_id;

	m_number = p_number;
	GPIOBASEMSG = BASEMSG;
	return true;
}

/*
* 默认高电平，默认初始化高电平为1；
* 触发为低电平；当从高变低的时候，复制为负的当前timestamp.ms数值；持续10ms后发消息msgon；发送结束后修改值为-1；-1不检查持续时间
* 变为高电平时，记录正的timestamp.ms数值；持续10ms后发消息msgoff；发送结束后修改值为1；1不检查持续时间
*/
DWORD __stdcall GPIO::MainWorkThread(LPVOID lpParam) {
//	MessageBox(NULL, L"GPIO线程启动!", L"GPIO", MB_OK);
	long requiredDur = 10;

	auto now = std::chrono::system_clock::now();
	auto timeMillis = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch());
	long long timeMillisCount = timeMillis.count();
	long long lastChange[8];
	for (int i = 0; i < 8; ++i) {
		lastChange[i] = timeMillisCount;
	}
	long pinLevel[8] = { 1, 0, 1, 0, 1, 0, 1, 0 };

	GPIO* gpio = static_cast<GPIO*>(lpParam);
	while (gpio->is_thread_running == true) {
		for (int i = 2; i < 8; i += 2) {
			// 检测 6, 4, 2
			unsigned char curState = 2;
			gpio->ReadMultipleTimes(i, curState, 10);

			if (curState == 0 && pinLevel[i] == 1) {
				// 高变低
				auto now = std::chrono::system_clock::now();
				auto timeMillis = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch());
				long long timeMillisCount = timeMillis.count();
				if (timeMillisCount - lastChange[i] < requiredDur) {
					continue;
				}

				// 触发
				if (gpio->msgmap.find(i) != gpio->msgmap.end()) {
					SendMessage(gpio->msgmap[i], gpio->GPIOBASEMSG + 1, i, i);
				}
				pinLevel[i] = 0;
				lastChange[i] = timeMillisCount;
			}
			if (curState == 1 && pinLevel[i] == 0) {
				// 低变高
				auto now = std::chrono::system_clock::now();
				auto timeMillis = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch());
				long long timeMillisCount = timeMillis.count();
				if (timeMillisCount - lastChange[i] < requiredDur) {
					continue;
				}

				// 触发结束
				if (gpio->msgmap.find(i) != gpio->msgmap.end()) {
					SendMessage(gpio->msgmap[i], gpio->GPIOBASEMSG - 1, i, i);
				}
				pinLevel[i] = 1;
				lastChange[i] = timeMillisCount;
			}
		}
	}

//	MessageBox(NULL, L"GPIO线程结束!", L"GPIO", MB_OK);
	return 0;
}

bool GPIO::StartThread() {
	if (m_handle == nullptr || is_thread_running) {
		return false;
	}
	thread_handle = CreateThread(NULL, 0, MainWorkThread, this, 0, NULL);
	is_thread_running = true;
	return true;
}

void GPIO::StopThread() {
	if (m_handle == nullptr || !is_thread_running) {
		return;
	}

	is_thread_running = false;
	WaitForSingleObject(thread_handle, INFINITE);
	CloseHandle(thread_handle); //注意在使用完句柄后要关闭它
	SendMessage(hwnd, this->GPIOBASEMSG, 0, 0);
}
