#pragma once
#include <iostream>
#include <chrono>
#include <unordered_map>

static std::unordered_map<std::string, int> deviceTypeCodemap = {
	{"GPIO",0},
	{"light",1},
	{"Camera",2},
	{"ScanningGun",3},
	{"Speaker",4},
	{"RemoteControl",5},
    {"Microphone",6}
};

/*static std::unordered_map<std::string, int> GPIOParammap = {
	{"devicelatency",0},
	{"light",1},
	{"Camera",2},
	{"ScanningGun",3},
	{"Speaker",4},
	{"RemoteControl",5}
};*/
static std::unordered_map<std::string, int> lightParammap = {
	{"devicelatency",0},//延迟开始工作时间（ms）
	{"linkedScanningGun",1},//扫码枪编码
	{"devicePin",2},//pin脚
	{"GPIODeviceCode",3},//GPIO设备号
};
static std::unordered_map<std::string, int> CameraParammap = {
	{"acquisitionFrameRate",0},//采集帧率
	{"exposureTime",1},//曝光时间
	{"gain",2},//增益
	{"devicelatency",3},//延迟开始工作时间（ms）
	{"acquisitionBurstFrameCount",4},//触发帧计数
	{"compressionQualit",5},//图片压缩质量
	{"camera_interval",6}
};
static std::unordered_map<std::string, int> ScanningGunParammap = {
	{"acquisitionFrameRate",0},//采集帧率
	{"devicelatency",1},//延迟开始工作时间（ms）
	{"exposureTime",2},//曝光时间 
	{"gain",3},//增益
	{"acquisitionBurstFrameCount",4},//触发帧计数
	{"lightSelectorEnable",5},//是否打开所有光源
	{"currentPosition",6}//电机步长（mm）
};
static std::unordered_map<std::string, int> AudioDeviceParammap = {
	{"devicelatency",0},//延迟开始工作时间（ms）
	{"audiofile1",1}//唤醒语音文件路径
};
static std::unordered_map<std::string, int> RemoteControlParammap = {
	{"devicelatency",0},//延迟开始工作时间（ms）
	{"baudRate",1},//波特率
	{"portName",2},//com口
	{"Message",3},//需发送的消息
	{"devicePin",4}//绑定的GPIO针脚
};

class equnit
{
public:
	std::string e_deviceTypeId;
	std::string e_deviceTypeName;
	std::string e_deviceTypeCode;
	std::string e_deviceCode;
	std::string e_deviceName;
public:
	equnit() {}
	equnit(std::string deviceTypeId, std::string deviceTypeName, std::string deviceTypeCode, std::string deviceCode, std::string deviceName) :e_deviceTypeId(deviceTypeId), e_deviceTypeName(deviceTypeName), e_deviceTypeCode(deviceTypeCode), e_deviceCode(deviceCode), e_deviceName(deviceName) {};
	virtual  ~equnit() {};
};