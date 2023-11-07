#ifndef _LIB_IOCTRL_H
#define _LIB_IOCTRL_H

#define   LBP_GROUPA_MASK                 0x01
#define   LBP_GROUPB_MASK                 0x02
#define   LBP_PUBLIC_MASK                 0x04

#define   PBS_SW1_MASK                    0x01
#define   PBS_SW2_MASK                    0x02
#define   PBS_SW3_MASK                    0x04
#define   PBS_SW4_MASK                    0x08

#define   GPIO_DYNAMIC_INOUT               0
#define   GPIO_FIXED_INOUT                 1

//typedef void* HMODULE;

typedef enum{
	GPIO_PIN1,
	GPIO_PIN2,
	GPIO_PIN3,
	GPIO_PIN4,
	GPIO_PIN5,
	GPIO_PIN6,
	GPIO_PIN7,
	GPIO_PIN8,
	GPIO_PIN9,
	GPIO_PIN10,
	GPIO_PIN11,
	GPIO_PIN12,
	GPIO_PIN13,
	GPIO_PIN14,
	GPIO_PIN15,
	GPIO_PIN16
}GPIO_PIN_INDEX;

typedef enum {
	WDT_SECOND_MODE,
	WDT_MINUTE_MODE
}WDT_TIMER_MODE;

typedef enum {
	DERECT_OUPUT,
	DERECT_INPUT
}GPIO_DERECTION_INFO;

typedef enum {
	GPIO_LOW_LEVEL,
	GPIO_HIGH_LEVEL
}GPIO_LEVEL;

typedef enum {
	BYPASS_GROUPA_INDEX,
	BYPASS_GROUPB_INDEX,
	BYPASS_PUBLIC_INDEX
}BYPASS_INDEX;

typedef enum {
	POWER_BUTTON1,
	POWER_BUTTON2,
	POWER_BUTTON3,
	POWER_BUTTON4
}POWERBUTTON_INDEX;


struct __WDT_INFO {
	unsigned char nowayout; //无退出标志，0：启动后可退出 1：启动后不可退出
	unsigned char mode;     //秒钟模式或者分钟模式
	unsigned char time;     //超时值
	unsigned char enable;   //配置完成后，立即启动看门狗
};



//init system driver
/*
**函数：libIoCtrlInit
**函数说明:初始化驱动及导入SDK动态库
**参数：hlib 模块地址指针，bios_id 主板BIOS ID号，在主板BIOS setup中可以查看得到、
**返回值: 0 成功，非0失败 ，成功时并导出void*的实例hlib
*/
extern "C" __declspec(dllexport) int libIoCtrlInit(void ** hlib, char* bios_id);


/*
**函数：libIoCtrlGetVersion
**函数说明:获取SDK动态库版本
**参数：hlib 模块地址指针，version SdK版本号
**返回值: 0 成功，非0失败 ，成功时导出demo sdk 的版本号(字符串)
*/
extern "C" __declspec(dllexport) int libIoCtrlGetVersion(void ** hlib, char* version);

//for gpio
/*
**函数：IsGpioSupported
**函数说明:查询项目是否支持GPIO功能
**
**参数：hlib 模块地址指针
*       isSupport 导出项目支持gpio状态,0为不支持，1为此项目支持gpio
**返回值: 0 成功，非0失败
*/
extern "C" __declspec(dllexport) int IsGpioSupported(const void ** hlib, unsigned char* isSupport);

/*
**函数：getGpioDerectionState
**函数说明:获取GPIO I/O 状态和数据状态
**参数：hlib 模块地址指针，*derection 导出的I/O 状态(bit0对应pin1,bit1对应pin2...) *currentState 导出的I/O 数据(bit0对应pin1,bit1对应pin2...) 
**      *fixed 导出GPIO输入输出固定模式(GPIO_FIXED_INOUT: 固定的输入输出模式 GPIO_DYNAMIC_INOUT：动态的，可配置的输入输出模式)
**返回值: 0 成功，非0失败
*/
extern "C" __declspec(dllexport) int getGpioState(void ** hlib, unsigned int* derection, unsigned int* currentState, unsigned char* fixed);

/*
**函数：getGpioMaxPin
**函数说明:获取支持的GPIO管脚数量
**参数：hlib 模块地址指针，pinnum 导出的管脚数量
**返回值: 0 成功，非0失败
*/
extern "C" __declspec(dllexport) int getGpioMaxPin(void ** hlib, unsigned char* pinnum);

/*
**函数：setPinInOut
**函数说明:配置管脚的输入输出
**参数：hlib 模块地址指针，index 管脚序号（参看GPIO_INDEX枚举定义），state 管脚输入输出设置(参看GPIO_INOUT的枚举定义)
**返回值: 0 成功，非0失败
*/
extern "C" __declspec(dllexport) int setPinInOut(void ** hlib, GPIO_PIN_INDEX index, GPIO_DERECTION_INFO state);
 
/*
**函数：getPinLevel
**函数说明:获取管脚的输入输出状态
**参数：hlib 模块地址指针，index 管脚序号（参看GPIO_INDEX枚举定义），curstate 导出的管脚输入输出状态,0为低电平，1为高电平
**返回值: 0 成功，非0失败
*/
extern "C" __declspec(dllexport) int getPinLevel(void ** hlib, GPIO_PIN_INDEX index, unsigned char* curstate);


/*
**函数：setPinLevel
**函数说明:设置管脚的输入输出状态
**参数：hlib 模块地址指针，index 管脚序号（参看GPIO_INDEX枚举定义），curstate 设置管脚输入输出状态,0为低电平，1为高电平(仅为寄存器状态，实际电路的状态，取决于外部电路)
**返回值: 0 成功，非0失败
*/
extern "C" __declspec(dllexport) int setPinLevel(void ** hlib, GPIO_PIN_INDEX index, GPIO_LEVEL curstate);

//for watchdog
/*
**函数：OpenWatchdog
**函数说明:设置看门狗
**参数：hlib 模块地址指针，wdtinfo 看门狗状态定义，请参看struct __WDT_INFO的定义
**返回值: 0 成功，非0失败
*/
extern "C" __declspec(dllexport) int OpenWatchdog(void ** hlib, struct __WDT_INFO* wdtinfo);

/*
**函数：updateWatchdog
**函数说明:更新看门狗定时器配置参数
**参数：hlib 模块地址指针，
**参数：wdtinfo 看门狗状态定义，请参看struct __WDT_INFO的定义
**返回值: 0 成功，非0失败
*/
extern "C" __declspec(dllexport) int updateWatchdog(void ** hlib, struct __WDT_INFO* wdtinfo);

/*
**函数：enableWatchdog
**函数说明:开启或者关闭看门狗，关闭操作取决于struct __WDT_INFO 的nowayout的配置，如果nowayout为1时，不会做关闭动作
**参数：hlib 模块地址指针，enValue 1 打开 0 关闭
**返回值: 0 成功，非0失败
*/
extern "C" __declspec(dllexport) int enableWatchdog(void ** hlib, unsigned char enValue);

/*
**函数：KeepWatchdogAlive
**函数说明:更新重置看门狗定时器(喂狗)
**         
**参数：hlib 模块地址指针
**返回值: 0 成功，非0失败
*/
extern "C" __declspec(dllexport) int KeepWatchdogAlive(void ** hlib);

/*
**函数：CloseWatchdogTimer
**函数说明:关闭看门狗定时器(依赖于OpenWatchdog传入的参数，如果nowayout为0，停止看门狗定时器，并结束本次调用，非0，仅结束本次调用，不关闭看门狗定时器)
**
**参数：hlib 模块地址指针，
**返回值: 0 成功，非0失败
*/
extern "C" __declspec(dllexport) int CloseWatchdog(const void ** hlib);


//Lan Bypass support

/*
**函数：IsLanBypassSupported
**函数说明:查询项目是否支持网络bypass功能
**
**参数：hlib 模块地址指针
*       isSupport 导出项目支持lanbypass状态,0为不支持，1为此项目支持Lan Bypass
**返回值: 0 成功，非0失败
*/
extern "C" __declspec(dllexport) int IsLanBypassSupported(const void ** hlib, unsigned char* isSupport);

/*
**函数：GetLanBypassState
**函数说明:查询Bypass支持组的状态
**
**参数：hlib 模块地址指针
*       state 导出lanbypass组状态,bit0为Group A的状态，bit1为Group B的状态 位值为0表示目前为关闭状态，1为开启状态
**返回值: 0 成功，非0失败
*/
extern "C" __declspec(dllexport) int GetLanBypassState(const void ** hlib, unsigned char* state);

/*
**函数：LanBypassEnable
**函数说明:开启或者关闭Lan bypass
**
**参数：hlib 模块地址指针
*       index lanbypass组序号(参考BYPASS_INDEX)
*		Enable  0：关闭 1：打开
**返回值: 0 成功，非0失败
*/
extern "C" __declspec(dllexport) int LanBypassEnable(const void ** hlib, BYPASS_INDEX index, unsigned char Enable);

//Power Button Switch support

/*
**函数：IsPowerButtonSWSupported
**函数说明:查询项目是否支持Power Button Switch功能
**
**参数：hlib 模块地址指针
*       isSupport 导出项目支持lanbypass状态,0为不支持，1为此项目支持Power Button Switch
**返回值: 0 成功，非0失败
*/
extern "C" __declspec(dllexport) int IsPowerButtonSWSupported(const void ** hlib, unsigned char* isSupport);

/*
**函数：GetPowerButtonSWAccessState
**函数说明:查询Bypass支持组的状态
**
**参数：hlib 模块地址指针
*       state 导出Power Button Switch组访问状态,bit0为Button1 的状态，bit1为Button2的状态 ....
*       mode  导出Power Button Switch组访问模式，Level=0 edge=1 bit0为Button1 的模式，bit1为Button2的模式 ...
**返回值: 0 成功，非0失败
*/
extern "C" __declspec(dllexport) int GetPowerButtonSWAccessState(const void ** hlib, unsigned char* state, unsigned char* mode);

/*
**函数：PowerButtonSWEnable
**函数说明:开启或者关闭Lan bypass
**
**参数：hlib 模块地址指针
*       index Button组序号(参考POWERBUTTON_INDEX)
*		Enable  0：关闭 1：打开 如果开关为POWER_BUTTON_EDGE模式，此参数不影响操作
**返回值: 0 成功，非0失败
*/
extern "C" __declspec(dllexport) int PowerButtonSWEnable(const void ** hlib, POWERBUTTON_INDEX index, unsigned char Enable);

/*
**函数：SysBeepEnable(const void ** hlib, unsigned short Frequency)
**函数说明:开启或者关闭System Buzzer
**
**参数：hlib 模块地址指针
*		Frequency  0：暂不适用
**返回值: 0 成功，非0失败
*/
extern "C" __declspec(dllexport) int SysBeepEnable(const void ** hlib, unsigned short Frequency);

//deinit system driver,exit
/*
**函数：libIoCtrlDeinit
**函数说明:卸载驱动及导入SDK动态库
**参数：hlib 模块地址指针
**返回值: 0 成功，非0失败
*/
extern "C" __declspec(dllexport) int libIoCtrlDeinit(void ** hlib);

#endif