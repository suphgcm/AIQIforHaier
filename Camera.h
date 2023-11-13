#pragma once
#include "equnit.h"
#include "MvCameraControl/MvCameraControl.h"
#include "MvCameraControl/CameraParams.h"
#include "nlohmann/json.hpp"

class Camera :
    public equnit
{
private:
    std::string IPADDR;
    void* m_handle = nullptr;
    MV_CC_DEVICE_INFO* m_mvDevInfo = nullptr;

    float m_exposureTime = 30000.0f; // ��λ����s
    float m_acquisitionFrameRate = 3.0f;
    float m_gain = 10.0f;
    int m_acquisitionBurstFrameCount = 1; // ����֡������������ʱ�Ķ���֡
    int m_compressionQuality = 80;  // temp default
    int m_cameraInterval = 100; // �Ķ���ʱ�ļ��ʱ�䣬��λ��ms

    bool m_isGot = false;
    bool m_isInited = false;
    bool m_isGrabbing = false;
    std::mutex m_mutex;

public:
    Camera() {}
    Camera(std::string IP_ADDR, std::string deviceTypeId, std::string deviceTypeName, std::string deviceTypeCode, std::string deviceCode, std::string deviceName) :
        equnit(deviceTypeId, deviceTypeName, deviceTypeCode, deviceCode, deviceName), IPADDR(IP_ADDR) {}
    ~Camera() { StopGrabbing(); Destroy(); }

    MV_CC_DEVICE_INFO* GetCameraByIpAddress();

    bool Init();
    bool Destroy();

    bool SetValuesForUninited(float exposureTime, float acquisitionFrameRate, float gain, int acquisitionBurstFrameCount, int compressionQuality, int cameraInterval);
    bool SetValuesForInited(float exposureTime, float acquisitionFrameRate, float gain, int acquisitionBurstFrameCount, int compressionQuality, int cameraInterval);
    bool SetValuesByJson(const nlohmann::json& deviceParamConfigList);

    bool StartGrabbing();
    bool StopGrabbing();

    bool GetImage(const std::string& path, void* args); // ָ���ļ��У��õ� jpeg ��ʽͼƬ���ϴ�
    void Lock();
    void UnLock();
    // ִ��˳��SetValuesByJson -> Init -> SetValuesByJson -> StartGrabbing -> GetImage -> StopGrabbing -> SetValuesByJson -> StartGrabbing -> GetImage -> StopGrabbing -> Destroy
};