#include "SerialCommunication.h"
#include <sstream>
#include <iomanip>

bool SerialCommunication::OpenSerial() {
	// 打开串口
	m_hCom = CreateFileA(m_portName.c_str(), // 串口名称
		GENERIC_READ | GENERIC_WRITE, // 允许读和写
		0, // 独占方式，串口不支持共享
		NULL, // 安全属性指针，默认值为NULL
		OPEN_EXISTING, // 打开现有的串口文件
		FILE_ATTRIBUTE_NORMAL, // 同步方式，创建一个非重叠文件
		NULL // 模板文件的句柄，用于复制文件属性，一般设为NULL
	);

	// 打开串口失败
	if (m_hCom == INVALID_HANDLE_VALUE) {
		DWORD dwError = GetLastError();  // 获取错误代码
		LPVOID lpMsgBuf;

		FormatMessage(          // 将错误代码转化为可读的错误信息
			FORMAT_MESSAGE_ALLOCATE_BUFFER |
			FORMAT_MESSAGE_FROM_SYSTEM |
			FORMAT_MESSAGE_IGNORE_INSERTS,
			NULL,
			dwError,
			MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
			(LPTSTR)&lpMsgBuf,
			0,
			NULL
		);

		std::cerr << "打开串口失败，错误信息 : " << (LPTSTR)lpMsgBuf << std::endl;
		LocalFree(lpMsgBuf);
		return false;
	}

	// 配置串口
	GetCommState(m_hCom, &m_dcb); // 获取当前串口配置
	m_dcb.BaudRate = m_baudRate; // 波特率
	m_dcb.ByteSize = 8; // 数据位，8
	m_dcb.Parity = NOPARITY; // 校验位，无
	m_dcb.StopBits = ONESTOPBIT; // 停止位，1
	SetCommState(m_hCom, &m_dcb); // 设置串口配置

	// 配置超时
	GetCommTimeouts(m_hCom, &m_timeOuts); // 获取当前超时参数
	m_timeOuts.ReadIntervalTimeout = 1000; // 读间隔超时
	m_timeOuts.ReadTotalTimeoutMultiplier = 500; // 读时间系数
	m_timeOuts.ReadTotalTimeoutConstant = 5000; // 读时间常量
	m_timeOuts.WriteTotalTimeoutMultiplier = 500; // 写时间系数
	m_timeOuts.WriteTotalTimeoutConstant = 2000; // 写时间常量
	SetCommTimeouts(m_hCom, &m_timeOuts); // 设置超时参数

	return true;
}

bool SerialCommunication::CloseSerial() const {
	if (m_hCom == INVALID_HANDLE_VALUE) {
		return true;
	}
	return CloseHandle(m_hCom);
}

bool SerialCommunication::SendMessage(const std::string& message) const {
	// 将16进制文本转换为字节对象
	std::stringstream ss;
	ss << std::hex << message; // 将16进制文本写入字符串流
	std::string byteStr;
	while (ss >> byteStr) {
		std::cout << byteStr << std::endl;
		unsigned char byte = (unsigned char)stoi(byteStr, nullptr, 16); // 将字节字符串转换为字节变量
		bool ret = WriteFile(m_hCom, // 串口句柄
			&byte, // 要发送的字节
			1, // 要发送的字节数
			nullptr, // 用于接收发送的字节数，NULL 表示不接收
			nullptr // 用于异步操作，NULL 表示同步操作
		); 
		if (!ret) {
			return false;
		}
	}
	return true;
}

bool SerialCommunication::SendMessage() const {
	SendMessage(m_message);
	return false;
}


bool SerialCommunication::ReceiveMessage(int length, std::string& message) const {
	// 定义一个字节缓冲区
	unsigned char buffer[1024];
	// 从串口读取数据
	bool ret = ReadFile(m_hCom, // 串口句柄
		buffer, // 数据缓冲区
		length, // 要读取的字节数
		nullptr, // 用于接收读取的字节数，NULL 表示不接收
		nullptr // 用于异步操作，NULL 表示同步操作
	);
	if (!ret) {
		return false;
	}

	// 将字节对象转换为16进制文本
	std::stringstream ss;
	for (int i = 0; i < length; i++) { // 遍历缓冲区中的字节
		ss << std::hex << std::setw(2) << std::setfill('0') << (int)buffer[i]; // 将字节转换为16进制文本并写入字符串流
	}
	message = ss.str();
	
	return true;
}