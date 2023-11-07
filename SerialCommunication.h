#ifndef _SERIAL_COMMUNICATION_H_
#define _SERIAL_COMMUNICATION_H_

#include "equnit.h"
#include <iostream>
#include <windows.h>
#include <string>

class SerialCommunication : public equnit {
private:
	std::string m_portName; // 串口名称，如 "COM3"
	int m_baudRate; // 波特率
	std::string m_message;
	int m_devicePin = 0;

	HANDLE m_hCom = nullptr; // 句柄
	DCB m_dcb;
	COMMTIMEOUTS m_timeOuts;

public:
	SerialCommunication(std::string deviceTypeId, std::string deviceTypeName, std::string deviceTypeCode, std::string deviceCode, std::string deviceName,
		const std::string& portName, const int baudRate, const std::string& message, const int devicePin) :
		equnit(deviceTypeId, deviceTypeName, deviceTypeCode, deviceCode, deviceName), m_portName(portName), m_baudRate(baudRate), m_message(message), m_devicePin(devicePin) {}
	~SerialCommunication() { CloseSerial(); }

	bool OpenSerial(); // 打开并配置串口
	bool CloseSerial() const; // 关闭串口
	bool SendMessage(const std::string& message) const; // 发送消息
	bool SendMessage() const;
	bool ReceiveMessage(int length, std::string& message) const; // 接收消息
};

#endif // !_SERIAL_COMMUNICATION_H_