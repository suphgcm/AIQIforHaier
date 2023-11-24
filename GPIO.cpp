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
* Ĭ�ϸߵ�ƽ��Ĭ�ϳ�ʼ���ߵ�ƽΪ1��
* ����Ϊ�͵�ƽ�����Ӹ߱�͵�ʱ�򣬸���Ϊ���ĵ�ǰtimestamp.ms��ֵ������10ms����Ϣmsgon�����ͽ������޸�ֵΪ-1��-1��������ʱ��
* ��Ϊ�ߵ�ƽʱ����¼����timestamp.ms��ֵ������10ms����Ϣmsgoff�����ͽ������޸�ֵΪ1��1��������ʱ��
*/
DWORD __stdcall GPIO::MainWorkThread(LPVOID lpParam) {
//	MessageBox(NULL, L"GPIO�߳�����!", L"GPIO", MB_OK);
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
			// ��� 6, 4, 2
			unsigned char curState = 2;
			gpio->ReadMultipleTimes(i, curState, 10);

			if (curState == 0 && pinLevel[i] == 1) {
				// �߱��
				auto now = std::chrono::system_clock::now();
				auto timeMillis = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch());
				long long timeMillisCount = timeMillis.count();
				if (timeMillisCount - lastChange[i] < requiredDur) {
					continue;
				}

				// ����
				if (gpio->msgmap.find(i) != gpio->msgmap.end()) {
					SendMessage(gpio->msgmap[i], gpio->GPIOBASEMSG + 1, i, i);
				}
				pinLevel[i] = 0;
				lastChange[i] = timeMillisCount;
			}
			if (curState == 1 && pinLevel[i] == 0) {
				// �ͱ��
				auto now = std::chrono::system_clock::now();
				auto timeMillis = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch());
				long long timeMillisCount = timeMillis.count();
				if (timeMillisCount - lastChange[i] < requiredDur) {
					continue;
				}

				// ��������
				if (gpio->msgmap.find(i) != gpio->msgmap.end()) {
					SendMessage(gpio->msgmap[i], gpio->GPIOBASEMSG - 1, i, i);
				}
				pinLevel[i] = 1;
				lastChange[i] = timeMillisCount;
			}
		}
	}

//	MessageBox(NULL, L"GPIO�߳̽���!", L"GPIO", MB_OK);
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
	CloseHandle(thread_handle); //ע����ʹ��������Ҫ�ر���
	SendMessage(hwnd, this->GPIOBASEMSG, 0, 0);
}
