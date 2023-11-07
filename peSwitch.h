#pragma once
#include "equnit.h"
#include "GPIO.h"

class peSwitch :
    public equnit
{
private:
    int pin;
 
    UINT peBaseMsg;
    std::string codereaderID;//devicecode of shotgun
    BOOLEAN b_istrigger;
    GPIO* gpUnit;
    HWND hwnd;
public:
    peSwitch(int GPIOpin,UINT BaseMsg, std::string l_codereaderID, BOOLEAN istrigger, GPIO* gpio, HWND whandle,
        std::string deviceTypeId, std::string deviceTypeName, std::string deviceTypeCode, std::string deviceCode, std::string deviceName) :
        equnit(deviceTypeId, deviceTypeName, deviceTypeCode, deviceCode, deviceName),
        pin(GPIOpin), peBaseMsg(BaseMsg), codereaderID(l_codereaderID), b_istrigger(istrigger), gpUnit(gpio), hwnd(whandle){      
        gpUnit->RegistDevice(pin, hwnd);
    }
    ~peSwitch() {}
    BOOLEAN istrigger() { return b_istrigger; }
    BOOLEAN registcodreader(std::unordered_map<std::string, equnit*> msgmap) {
    }
    std::string triggergetcode();
};

