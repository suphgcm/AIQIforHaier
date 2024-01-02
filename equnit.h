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
	{"devicelatency",0},//�ӳٿ�ʼ����ʱ�䣨ms��
	{"linkedScanningGun",1},//ɨ��ǹ����
	{"devicePin",2},//pin��
	{"GPIODeviceCode",3},//GPIO�豸��
};
static std::unordered_map<std::string, int> CameraParammap = {
	{"acquisitionFrameRate",0},//�ɼ�֡��
	{"exposureTime",1},//�ع�ʱ��
	{"gain",2},//����
	{"devicelatency",3},//�ӳٿ�ʼ����ʱ�䣨ms��
	{"acquisitionBurstFrameCount",4},//����֡����
	{"compressionQualit",5},//ͼƬѹ������
	{"camera_interval",6}
};
static std::unordered_map<std::string, int> ScanningGunParammap = {
	{"acquisitionFrameRate",0},//�ɼ�֡��
	{"devicelatency",1},//�ӳٿ�ʼ����ʱ�䣨ms��
	{"exposureTime",2},//�ع�ʱ�� 
	{"gain",3},//����
	{"acquisitionBurstFrameCount",4},//����֡����
	{"lightSelectorEnable",5},//�Ƿ�����й�Դ
	{"currentPosition",6}//���������mm��
};
static std::unordered_map<std::string, int> AudioDeviceParammap = {
	{"devicelatency",0},//�ӳٿ�ʼ����ʱ�䣨ms��
	{"audiofile1",1}//���������ļ�·��
};
static std::unordered_map<std::string, int> RemoteControlParammap = {
	{"devicelatency",0},//�ӳٿ�ʼ����ʱ�䣨ms��
	{"baudRate",1},//������
	{"portName",2},//com��
	{"Message",3},//�跢�͵���Ϣ
	{"devicePin",4}//�󶨵�GPIO���
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