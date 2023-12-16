#pragma once
#include "equnit.h"
#include "MvCodeReaderCtrl/MvCodeReaderCtrl.h"
#include "MvCodeReaderCtrl/MvCodeReaderParams.h"
#include <vector>
#include "nlohmann/json.hpp"

class CodeReader :
    public equnit
{
private:
    std::string cIPADDR;
    void* m_handle = nullptr;
    MV_CODEREADER_DEVICE_INFO* m_mvDevInfo = nullptr;

    float m_exposureTime = 1000.0f;
    float m_acquisitionFrameRate = 3.0f;
    float m_gain = 10.0f;
    int m_acquisitionBurstFrameCount = 1;
    int m_lightSelectorEnable = 1;
    int m_currentPosition = 900;

    bool m_isGot = false;
    bool m_isInited = false;
    bool m_isGrabbing = false;
    std::mutex m_mutex;

public:
    CodeReader() {}
    CodeReader(std::string IP_ADDR, std::string deviceTypeId, std::string deviceTypeName, std::string deviceTypeCode, std::string deviceCode, std::string deviceName) :
        equnit(deviceTypeId, deviceTypeName, deviceTypeCode, deviceCode, deviceName), cIPADDR(IP_ADDR) {}
    ~CodeReader() { StopGrabbing(); Destroy(); }

    MV_CODEREADER_DEVICE_INFO* GetCodeReaderByIpAddress();

    bool Init();
    bool Destroy();

    bool SetValuesForUninited(float exposureTime, float acquisitionFrameRate, float gain, int acquisitionBurstFrameCount, int lightSelectorEnable, int currentPosition);
    bool SetValuesForInited(float exposureTime, float acquisitionFrameRate, float gain, int acquisitionBurstFrameCount, int lightSelectorEnable, int currentPosition);
    bool SetValuesByJson(const nlohmann::json& deviceParamConfigList);

    bool StartGrabbing();
    bool StopGrabbing();

    int ReadCode(std::vector<std::string>& results) const;
    
    int GetAcquisitionBurstFrameCount();
    void* GetHandle();
    void Lock();
    void UnLock();
    // Ö´ÐÐË³Ðò£ºSetValuesByJson -> Init -> SetValuesByJson -> StartGrabbing -> ReadCode -> SetValuesByJson -> SetValuesForInited -> StartGrabbing -> ReadCode -> StopGrabbing -> Destroy
};