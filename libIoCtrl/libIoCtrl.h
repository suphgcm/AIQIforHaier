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
	unsigned char nowayout; //���˳���־��0����������˳� 1�������󲻿��˳�
	unsigned char mode;     //����ģʽ���߷���ģʽ
	unsigned char time;     //��ʱֵ
	unsigned char enable;   //������ɺ������������Ź�
};



//init system driver
/*
**������libIoCtrlInit
**����˵��:��ʼ������������SDK��̬��
**������hlib ģ���ַָ�룬bios_id ����BIOS ID�ţ�������BIOS setup�п��Բ鿴�õ���
**����ֵ: 0 �ɹ�����0ʧ�� ���ɹ�ʱ������void*��ʵ��hlib
*/
extern "C" __declspec(dllexport) int libIoCtrlInit(void ** hlib, char* bios_id);


/*
**������libIoCtrlGetVersion
**����˵��:��ȡSDK��̬��汾
**������hlib ģ���ַָ�룬version SdK�汾��
**����ֵ: 0 �ɹ�����0ʧ�� ���ɹ�ʱ����demo sdk �İ汾��(�ַ���)
*/
extern "C" __declspec(dllexport) int libIoCtrlGetVersion(void ** hlib, char* version);

//for gpio
/*
**������IsGpioSupported
**����˵��:��ѯ��Ŀ�Ƿ�֧��GPIO����
**
**������hlib ģ���ַָ��
*       isSupport ������Ŀ֧��gpio״̬,0Ϊ��֧�֣�1Ϊ����Ŀ֧��gpio
**����ֵ: 0 �ɹ�����0ʧ��
*/
extern "C" __declspec(dllexport) int IsGpioSupported(const void ** hlib, unsigned char* isSupport);

/*
**������getGpioDerectionState
**����˵��:��ȡGPIO I/O ״̬������״̬
**������hlib ģ���ַָ�룬*derection ������I/O ״̬(bit0��Ӧpin1,bit1��Ӧpin2...) *currentState ������I/O ����(bit0��Ӧpin1,bit1��Ӧpin2...) 
**      *fixed ����GPIO��������̶�ģʽ(GPIO_FIXED_INOUT: �̶����������ģʽ GPIO_DYNAMIC_INOUT����̬�ģ������õ��������ģʽ)
**����ֵ: 0 �ɹ�����0ʧ��
*/
extern "C" __declspec(dllexport) int getGpioState(void ** hlib, unsigned int* derection, unsigned int* currentState, unsigned char* fixed);

/*
**������getGpioMaxPin
**����˵��:��ȡ֧�ֵ�GPIO�ܽ�����
**������hlib ģ���ַָ�룬pinnum �����Ĺܽ�����
**����ֵ: 0 �ɹ�����0ʧ��
*/
extern "C" __declspec(dllexport) int getGpioMaxPin(void ** hlib, unsigned char* pinnum);

/*
**������setPinInOut
**����˵��:���ùܽŵ��������
**������hlib ģ���ַָ�룬index �ܽ���ţ��ο�GPIO_INDEXö�ٶ��壩��state �ܽ������������(�ο�GPIO_INOUT��ö�ٶ���)
**����ֵ: 0 �ɹ�����0ʧ��
*/
extern "C" __declspec(dllexport) int setPinInOut(void ** hlib, GPIO_PIN_INDEX index, GPIO_DERECTION_INFO state);
 
/*
**������getPinLevel
**����˵��:��ȡ�ܽŵ��������״̬
**������hlib ģ���ַָ�룬index �ܽ���ţ��ο�GPIO_INDEXö�ٶ��壩��curstate �����Ĺܽ��������״̬,0Ϊ�͵�ƽ��1Ϊ�ߵ�ƽ
**����ֵ: 0 �ɹ�����0ʧ��
*/
extern "C" __declspec(dllexport) int getPinLevel(void ** hlib, GPIO_PIN_INDEX index, unsigned char* curstate);


/*
**������setPinLevel
**����˵��:���ùܽŵ��������״̬
**������hlib ģ���ַָ�룬index �ܽ���ţ��ο�GPIO_INDEXö�ٶ��壩��curstate ���ùܽ��������״̬,0Ϊ�͵�ƽ��1Ϊ�ߵ�ƽ(��Ϊ�Ĵ���״̬��ʵ�ʵ�·��״̬��ȡ�����ⲿ��·)
**����ֵ: 0 �ɹ�����0ʧ��
*/
extern "C" __declspec(dllexport) int setPinLevel(void ** hlib, GPIO_PIN_INDEX index, GPIO_LEVEL curstate);

//for watchdog
/*
**������OpenWatchdog
**����˵��:���ÿ��Ź�
**������hlib ģ���ַָ�룬wdtinfo ���Ź�״̬���壬��ο�struct __WDT_INFO�Ķ���
**����ֵ: 0 �ɹ�����0ʧ��
*/
extern "C" __declspec(dllexport) int OpenWatchdog(void ** hlib, struct __WDT_INFO* wdtinfo);

/*
**������updateWatchdog
**����˵��:���¿��Ź���ʱ�����ò���
**������hlib ģ���ַָ�룬
**������wdtinfo ���Ź�״̬���壬��ο�struct __WDT_INFO�Ķ���
**����ֵ: 0 �ɹ�����0ʧ��
*/
extern "C" __declspec(dllexport) int updateWatchdog(void ** hlib, struct __WDT_INFO* wdtinfo);

/*
**������enableWatchdog
**����˵��:�������߹رտ��Ź����رղ���ȡ����struct __WDT_INFO ��nowayout�����ã����nowayoutΪ1ʱ���������رն���
**������hlib ģ���ַָ�룬enValue 1 �� 0 �ر�
**����ֵ: 0 �ɹ�����0ʧ��
*/
extern "C" __declspec(dllexport) int enableWatchdog(void ** hlib, unsigned char enValue);

/*
**������KeepWatchdogAlive
**����˵��:�������ÿ��Ź���ʱ��(ι��)
**         
**������hlib ģ���ַָ��
**����ֵ: 0 �ɹ�����0ʧ��
*/
extern "C" __declspec(dllexport) int KeepWatchdogAlive(void ** hlib);

/*
**������CloseWatchdogTimer
**����˵��:�رտ��Ź���ʱ��(������OpenWatchdog����Ĳ��������nowayoutΪ0��ֹͣ���Ź���ʱ�������������ε��ã���0�����������ε��ã����رտ��Ź���ʱ��)
**
**������hlib ģ���ַָ�룬
**����ֵ: 0 �ɹ�����0ʧ��
*/
extern "C" __declspec(dllexport) int CloseWatchdog(const void ** hlib);


//Lan Bypass support

/*
**������IsLanBypassSupported
**����˵��:��ѯ��Ŀ�Ƿ�֧������bypass����
**
**������hlib ģ���ַָ��
*       isSupport ������Ŀ֧��lanbypass״̬,0Ϊ��֧�֣�1Ϊ����Ŀ֧��Lan Bypass
**����ֵ: 0 �ɹ�����0ʧ��
*/
extern "C" __declspec(dllexport) int IsLanBypassSupported(const void ** hlib, unsigned char* isSupport);

/*
**������GetLanBypassState
**����˵��:��ѯBypass֧�����״̬
**
**������hlib ģ���ַָ��
*       state ����lanbypass��״̬,bit0ΪGroup A��״̬��bit1ΪGroup B��״̬ λֵΪ0��ʾĿǰΪ�ر�״̬��1Ϊ����״̬
**����ֵ: 0 �ɹ�����0ʧ��
*/
extern "C" __declspec(dllexport) int GetLanBypassState(const void ** hlib, unsigned char* state);

/*
**������LanBypassEnable
**����˵��:�������߹ر�Lan bypass
**
**������hlib ģ���ַָ��
*       index lanbypass�����(�ο�BYPASS_INDEX)
*		Enable  0���ر� 1����
**����ֵ: 0 �ɹ�����0ʧ��
*/
extern "C" __declspec(dllexport) int LanBypassEnable(const void ** hlib, BYPASS_INDEX index, unsigned char Enable);

//Power Button Switch support

/*
**������IsPowerButtonSWSupported
**����˵��:��ѯ��Ŀ�Ƿ�֧��Power Button Switch����
**
**������hlib ģ���ַָ��
*       isSupport ������Ŀ֧��lanbypass״̬,0Ϊ��֧�֣�1Ϊ����Ŀ֧��Power Button Switch
**����ֵ: 0 �ɹ�����0ʧ��
*/
extern "C" __declspec(dllexport) int IsPowerButtonSWSupported(const void ** hlib, unsigned char* isSupport);

/*
**������GetPowerButtonSWAccessState
**����˵��:��ѯBypass֧�����״̬
**
**������hlib ģ���ַָ��
*       state ����Power Button Switch�����״̬,bit0ΪButton1 ��״̬��bit1ΪButton2��״̬ ....
*       mode  ����Power Button Switch�����ģʽ��Level=0 edge=1 bit0ΪButton1 ��ģʽ��bit1ΪButton2��ģʽ ...
**����ֵ: 0 �ɹ�����0ʧ��
*/
extern "C" __declspec(dllexport) int GetPowerButtonSWAccessState(const void ** hlib, unsigned char* state, unsigned char* mode);

/*
**������PowerButtonSWEnable
**����˵��:�������߹ر�Lan bypass
**
**������hlib ģ���ַָ��
*       index Button�����(�ο�POWERBUTTON_INDEX)
*		Enable  0���ر� 1���� �������ΪPOWER_BUTTON_EDGEģʽ���˲�����Ӱ�����
**����ֵ: 0 �ɹ�����0ʧ��
*/
extern "C" __declspec(dllexport) int PowerButtonSWEnable(const void ** hlib, POWERBUTTON_INDEX index, unsigned char Enable);

/*
**������SysBeepEnable(const void ** hlib, unsigned short Frequency)
**����˵��:�������߹ر�System Buzzer
**
**������hlib ģ���ַָ��
*		Frequency  0���ݲ�����
**����ֵ: 0 �ɹ�����0ʧ��
*/
extern "C" __declspec(dllexport) int SysBeepEnable(const void ** hlib, unsigned short Frequency);

//deinit system driver,exit
/*
**������libIoCtrlDeinit
**����˵��:ж������������SDK��̬��
**������hlib ģ���ַָ��
**����ֵ: 0 �ɹ�����0ʧ��
*/
extern "C" __declspec(dllexport) int libIoCtrlDeinit(void ** hlib);

#endif