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

    float m_exposureTime = 30000.0f; // 单位：μs
    float m_acquisitionFrameRate = 3.0f;
    float m_gain = 10.0f;
    int m_acquisitionBurstFrameCount = 1; // 触发帧计数，即触发时拍多少帧
    int m_compressionQuality = 80;  // temp default
    int m_cameraInterval = 100; // 拍多张时的间隔时间，单位：ms

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

    bool GetImage(const std::string& path, void* args); // 指定文件夹，得到 jpeg 格式图片并上传
    void Lock();
    void UnLock();
    // 执行顺序：SetValuesByJson -> Init -> SetValuesByJson -> StartGrabbing -> GetImage -> StopGrabbing -> SetValuesByJson -> StartGrabbing -> GetImage -> StopGrabbing -> Destroy
};