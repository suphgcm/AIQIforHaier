

#if DLL_EXPORTS == 1
#define MYDLL_API __declspec(dllexport)
#else
#define MYDLL_API __declspec(dllimport)

#define ENABLE				1
#define DISABLE				0


#define PCH_GPIO_SUPPORT	ENABLE	

#define SMBUS_SUPPORT		DISABLE

#define SIO_SUPPORT			DISABLE
#define SIO_GPIO_SUPPORT	DISABLE	
#define SIO_WDT_SUPPORT		DISABLE

#define SCE_SUPPORT			DISABLE


class WinIOHelper;

#if PCH_GPIO_SUPPORT == ENABLE || SIO_GPIO_SUPPORT == ENABLE
enum GPIO_STATE {
	GPIO_INPUT = 0,
	GPIO_LOW,
	GPIO_HIGH
};
#endif 




#if PCH_GPIO_SUPPORT
class PchHelper;
#endif 

#if SIO_SUPPORT == ENABLE
class SuperIOHelper;
#endif 

#if SMBUS_SUPPORT == ENABLE
class SmbusHelper;

struct OffsetAndBit;
#endif 

#endif



class MYDLL_API Uhi {

private:



#if PCH_GPIO_SUPPORT == ENABLE

	PchHelper* pch = nullptr;

#endif 

#if SIO_SUPPORT == ENABLE

	SuperIOHelper* sio = nullptr;

#endif 

#if SCE_SUPPORT == ENABLE

	SCEHelper* sce;

#endif 


#if SMBUS_SUPPORT == ENABLE

	const static OffsetAndBit offsetAndBitList[16];

	SmbusHelper* smbus = nullptr;

#endif 

#if RTC_SUPPORT == ENABLE 
	RTCHelper *rtc = nullptr;
#endif


public:

	Uhi();

	~Uhi();

	BOOL GetWinIoInitializeStates();

#if PCH_GPIO_SUPPORT == ENABLE

	BOOL SetGpioState(UINT8 GpioNum, GPIO_STATE GpioState);

	BOOL GetGpioState(UINT8 GpioNum, GPIO_STATE& GpioState);

#endif 

#if SIO_SUPPORT == ENABLE && SIO_GPIO_SUPPORT == ENABLE

	BOOL SioSetGpioState(UINT8 GpioNum, GPIO_STATE& GpioState);

#endif 

#if SIO_SUPPORT == ENABLE && SIO_WDT_SUPPORT == ENABLE
	VOID SetWdt(UINT16 Timer);

#endif

#if SMBUS_SUPPORT == ENABLE

	BOOL SmbusSetGpioState(UINT8 GpioNum, GPIO_STATE GpioState);

#endif 

#if RTC_SUPPORT == ENABLE 
	BOOL DynamicMin(IN UINT8 DynamicMinIncrease);

	BOOL FixTime(IN UINT8 RTCWakeupTimeHour, IN UINT8 RTCWakeupTimeMinute, IN UINT8 RTCWakeupTimeSecond);
#endif

#if GENERAL_SUPPORT	== ENABLE

	//reboot computer
	VOID Reboot();

	//to s5
	VOID ShutDown();

	//to s3
	VOID Sleep();

	//to s4
	VOID Hibernate();

#endif

};