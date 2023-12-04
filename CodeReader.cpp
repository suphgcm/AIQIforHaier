#include "CodeReader.h"
#include <string>
#include <chrono>
#include <thread>
#include <windows.h>

extern void AppendLog(LPCWSTR text);
extern LPCWSTR StringToLPCWSTR(const std::string& s);

MV_CODEREADER_DEVICE_INFO* CodeReader::GetCodeReaderByIpAddress() {
	if (m_isGot) {
		return m_mvDevInfo;
	}

	MV_CODEREADER_DEVICE_INFO_LIST stDeviceList;
	memset(&stDeviceList, 0, sizeof(MV_CODEREADER_DEVICE_INFO_LIST));

	int nRet = MV_CODEREADER_EnumDevices(&stDeviceList, MV_CODEREADER_GIGE_DEVICE);
	if (nRet != MV_CODEREADER_OK) {
		std::cerr << "Enum code readers failed! nRet [0x" << std::hex << nRet << std::dec << "]" << std::endl;
		return nullptr;
	}
	if (stDeviceList.nDeviceNum <= 0) {
		std::cerr << "No code readers found!" << std::endl;
		return nullptr;
	}
	for (unsigned int i = 0; i < stDeviceList.nDeviceNum; i++) {
		std::cout << "CodeReader[" << i << "]:" << std::endl;
		MV_CODEREADER_DEVICE_INFO* pstMVDevInfo = stDeviceList.pDeviceInfo[i];
		if (pstMVDevInfo == nullptr) {
			break;
		}

		if (pstMVDevInfo->nTLayerType == MV_CODEREADER_GIGE_DEVICE) {
			std::string name(reinterpret_cast<const char*>(pstMVDevInfo->SpecialInfo.stGigEInfo.chUserDefinedName), 16);
			std::cout << "UserDefinedName: " << name << std::endl;

			std::string serialNumber(reinterpret_cast<const char*>(pstMVDevInfo->SpecialInfo.stGigEInfo.chSerialNumber), 16);
			std::cout << "SerialNumber: " << serialNumber << std::endl;

			int nIp1 = ((pstMVDevInfo->SpecialInfo.stGigEInfo.nCurrentIp & 0xff000000) >> 24);
			int nIp2 = ((pstMVDevInfo->SpecialInfo.stGigEInfo.nCurrentIp & 0x00ff0000) >> 16);
			int nIp3 = ((pstMVDevInfo->SpecialInfo.stGigEInfo.nCurrentIp & 0x0000ff00) >> 8);
			int nIp4 = (pstMVDevInfo->SpecialInfo.stGigEInfo.nCurrentIp & 0x000000ff);
			std::string ipAddress = std::to_string(nIp1) + '.' + std::to_string(nIp2) + '.' + std::to_string(nIp3) + '.' + std::to_string(nIp4);
			std::cout << "CurrentIp: " << ipAddress << std::endl;
			if (ipAddress._Equal(cIPADDR)) {
				m_mvDevInfo = pstMVDevInfo;
				m_isGot = true;
				return pstMVDevInfo;
			}
			
		}
		else if (pstMVDevInfo->nTLayerType == MV_CODEREADER_USB_DEVICE) {
			std::cerr << "pstMVDevInfo->nTLayerType == MV_CODEREADER_USB_DEVICE, wrong!" << std::endl;
		}
		else {
			std::cerr << "pstMVDevInfo->nTLayerType does not support." << std::endl;
		}
	}
	return NULL;
}

bool CodeReader::Init() {
	if (m_isInited) {
		return true;
	}
	if (!m_isGot) {
		std::cerr << "Code reader is not got yet." << std::endl;
		return false;
	}

	// 创建句柄
	int nRet = MV_CODEREADER_CreateHandle(&m_handle, m_mvDevInfo);
	if (nRet != MV_CODEREADER_OK) {
		printf("Create Handle fail! nRet [0x%x]\n", nRet);
		return false;
	}

	// 连接设备 todo: 考虑中间出现 false 时的析构
	nRet = MV_CODEREADER_OpenDevice(m_handle);
	if (nRet != MV_CODEREADER_OK) {
		printf("Open Device fail! nRet [0x%x]\n", nRet);
		return false;
	}

	// 设置触发模式为 off todo: on?
	nRet = MV_CODEREADER_SetEnumValue(m_handle, "TriggerMode", MV_CODEREADER_TRIGGER_MODE_OFF);
	if (nRet != MV_CODEREADER_OK) {
		printf("Set Trigger Mode fail! nRet [0x%x]\n", nRet);
		return false;
	}

	// 设置关闭自动曝光
	nRet = MV_CODEREADER_SetEnumValue(m_handle, "ExposureAuto", 0);
	if (nRet != MV_CODEREADER_OK) {
		printf("Set ExposureAuto fail! nRet [0x%x]\n", nRet);
		return false;
	}

	// 设置曝光时间，设置曝光时间前请先关闭自动曝光，否则会设置失败
	nRet = MV_CODEREADER_SetFloatValue(m_handle, "ExposureTime", m_exposureTime);
	if (nRet != MV_CODEREADER_OK) {
		printf("Set ExposureTime fail! nRet [0x%x]\n", nRet);
	}

	// 设置采集帧率
	nRet = MV_CODEREADER_SetFloatValue(m_handle, "AcquisitionFrameRate", m_acquisitionFrameRate);
	if (nRet != MV_CODEREADER_OK) {
		printf("Set AcquisitionFrameRate fail! nRet [0x%x]\n", nRet);
	}

	// 应该不需要采集帧率控制

	// 设置自动曝光增益
	nRet = MV_CODEREADER_SetEnumValue(m_handle, "GainAuto", 0);
	if (nRet != MV_CODEREADER_OK) {
		printf("Set GainAuto fail [%x]\n", nRet);
	}

	// 设置曝光增益，请先关闭自动曝光增益否则失败
	nRet = MV_CODEREADER_SetFloatValue(m_handle, "Gain", m_gain);
	if (nRet != MV_CODEREADER_OK) {
		printf("Set Gain fail! nRet [0x%x]\n", nRet);
	}

	// 设置光源
	if (m_lightSelectorEnable == 1) {
		nRet = MV_CODEREADER_SetCommandValue(m_handle, "LightingAllEnable");
	}
	else {
		nRet = MV_CODEREADER_SetCommandValue(m_handle, "LightingAllDisEnable");
	}
	if (nRet != MV_CODEREADER_OK) {
		printf("Set light fail! nRet [0x%x]\n", nRet);
	}

	//// 设置电机步长 todo: 调整
	//nRet = MV_CODEREADER_SetIntValue(m_handle, "CurrentStep", m_currentPosition);
	//if (nRet != MV_CODEREADER_OK) {
	//	printf("Set CurrentStep fail! nRet [0x%x]\n", nRet);
	//}

	m_isInited = true;
	return true;
}

bool CodeReader::Destroy() {
	if (!m_isInited) {
		return true;
	}
	if (m_isGrabbing) {
		StopGrabbing();
	}

	// 关闭连接
	int nRet = MV_CODEREADER_CloseDevice(m_handle);
	if (nRet != MV_CODEREADER_OK) {
		printf("ClosDevice fail! nRet [0x%x]\n", nRet);
		return false;
	}

	// 销毁句柄
	nRet = MV_CODEREADER_DestroyHandle(m_handle);
	while (nRet != MV_CODEREADER_OK) {
		printf("Destroy Handle fail! nRet [0x%x]\n", nRet);
		nRet = MV_CODEREADER_DestroyHandle(m_handle);
	}

	m_handle = nullptr;
	m_isInited = false;
	return true;
}
void* CodeReader::GetHandle() 
{
	return m_handle;
}
bool CodeReader::SetValuesForUninited(
	float exposureTime, float acquisitionFrameRate, float gain, int acquisitionBurstFrameCount, 
	int lightSelectorEnable, int currentPosition
) {
	if (m_isInited) {
		return true;
	}

	m_exposureTime = exposureTime;
	m_acquisitionFrameRate = acquisitionFrameRate;
	m_gain = gain;
	m_acquisitionBurstFrameCount = acquisitionBurstFrameCount;
	m_lightSelectorEnable = lightSelectorEnable;
	m_currentPosition = currentPosition;

	return false;
}

bool CodeReader::SetValuesForInited(
	float exposureTime, float acquisitionFrameRate, float gain, int acquisitionBurstFrameCount, 
	int lightSelectorEnable, int currentPosition
) {
	if (!m_isInited) {
		return false;
	}

	// 设置曝光时间
	int nRet = MV_CODEREADER_SetFloatValue(m_handle, "ExposureTime", exposureTime);
	if (nRet != MV_CODEREADER_OK) {
		printf("Set ExposureTime fail! nRet [0x%x]\n", nRet);
		return false;
	}
	m_exposureTime = exposureTime;

	// 设置采集帧率
	nRet = MV_CODEREADER_SetFloatValue(m_handle, "AcquisitionFrameRate", acquisitionFrameRate);
	if (nRet != MV_CODEREADER_OK) {
		printf("Set AcquisitionFrameRate fail! nRet [0x%x]\n", nRet);
		return false;
	}
	m_acquisitionFrameRate = acquisitionFrameRate;

	// 设置曝光增益，请先关闭自动曝光增益否则失败
	nRet = MV_CODEREADER_SetFloatValue(m_handle, "Gain", gain);
	if (nRet != MV_CODEREADER_OK) {
		printf("Set Gain fail! nRet [0x%x]\n", nRet);
		return false;
	}
	m_gain = gain;

	// 采集帧计数
	m_acquisitionBurstFrameCount = acquisitionBurstFrameCount;

	// 设置光源
	if (lightSelectorEnable == 1) {
		nRet = MV_CODEREADER_SetCommandValue(m_handle, "LightingAllEnable");
	}
	else if (lightSelectorEnable == 0) {
		nRet = MV_CODEREADER_SetCommandValue(m_handle, "LightingAllDisEnable");
	}
	else {
		printf("Light arg invalid! nRet [0x%x]\n", nRet);
		return false;
	}
	if (nRet != MV_CODEREADER_OK) {
		printf("Set light fail! nRet [0x%x]\n", nRet);
		return false;
	}
	m_lightSelectorEnable = lightSelectorEnable;

	//// 设置电机步长 todo: 调整
	//nRet = MV_CODEREADER_SetIntValue(m_handle, "CurrentStep", currentPosition);
	//if (nRet != MV_CODEREADER_OK) {
	//	printf("Set CurrentStep fail! nRet [0x%x]\n", nRet);
	//	// return false;
	//}
	m_currentPosition = currentPosition;

	return true;
}

bool CodeReader::SetValuesByJson(const nlohmann::json& deviceParamConfigList) {
	float exposureTime = m_exposureTime;
	float acquisitionFrameRate = m_acquisitionFrameRate;
	float gain = m_gain;
	int acquisitionBurstFrameCount = m_acquisitionBurstFrameCount;
	int lightSelectorEnable = m_lightSelectorEnable;
	int currentPosition = m_currentPosition;

	for (auto deviceParam : deviceParamConfigList) {
		switch (ScanningGunParammap[deviceParam["paramCode"]]) {
		case 0:
			acquisitionFrameRate = std::stof((std::string)deviceParam["paramValue"]);
			break;
		case 1:
			// devicelatency = deviceParam["paramValue"];
			break;
		case 2:
			exposureTime = std::stof((std::string)deviceParam["paramValue"]);
			break;
		case 3:
			gain = std::stof((std::string)deviceParam["paramValue"]);
			break;
		case 4:
			acquisitionBurstFrameCount = std::stoi((std::string)deviceParam["paramValue"]);
			break;
		case 5:
			lightSelectorEnable = std::stoi((std::string)deviceParam["paramValue"]);
			break;
		case 6:
			currentPosition = std::stoi((std::string)deviceParam["paramValue"]);
			break;
		default:
			break;
		}
	}

	if (m_isInited) {
		return SetValuesForInited(exposureTime, acquisitionFrameRate, gain, acquisitionBurstFrameCount, lightSelectorEnable, currentPosition);
	}
	return SetValuesForUninited(exposureTime, acquisitionFrameRate, gain, acquisitionBurstFrameCount, lightSelectorEnable, currentPosition);
}


bool CodeReader::StartGrabbing() {
	if (m_isGrabbing) {
		return true;
	}
	if (!m_isInited) {
		std::cerr << "Code reader is not inited yet." << std::endl;
		return false;
	}

	int nRet = MV_CODEREADER_StartGrabbing(m_handle);
	if (nRet != MV_CODEREADER_OK) {
		printf("Start Grabbing fail! nRet [0x%x]\n", nRet);
		return false;
	}

	m_isGrabbing = true;
	return true;
}


bool CodeReader::StopGrabbing() {
	return true;
	if (!m_isGrabbing) {
		return true;
	}

	int nRet = MV_CODEREADER_StopGrabbing(m_handle);
	if (nRet != MV_CODEREADER_OK) {
		printf("Stop Grabbing fail! nRet [0x%x]\n", nRet);
		return false;
	}

	m_isGrabbing = false;
	return true;
}

int CodeReader::ReadCode(std::vector<std::string>& codes) const {


	codes.clear();
	for (int i = 0; i < m_acquisitionBurstFrameCount; ++i) {	
	MV_CODEREADER_IMAGE_OUT_INFO_EX2 stImageInfo = { 0 };
	memset(&stImageInfo, 0, sizeof(MV_CODEREADER_IMAGE_OUT_INFO_EX2));
	unsigned char* pData = NULL;

	auto now = std::chrono::system_clock::now();
	auto timeMillis = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch());
	long long timeMillisCount = timeMillis.count();

		int nRet = MV_CODEREADER_GetOneFrameTimeoutEx2(m_handle, &pData, &stImageInfo, 1000);
		if (nRet != MV_CODEREADER_OK) {
			printf("No data[0x%x]\n", nRet);
			// gcm 
			//return nRet;
			auto now2 = std::chrono::system_clock::now();
			auto timeMillis2 = std::chrono::duration_cast<std::chrono::milliseconds>(now2.time_since_epoch());
			long long timeMillisCount2 = timeMillis2.count();
			std::string Log = "Scan code ret = "+ std::to_string(nRet) + "Scan Code time = " + std::to_string(timeMillisCount2 - timeMillisCount) + "\n";
			AppendLog(StringToLPCWSTR(Log));
			continue;
		}
		auto now1 = std::chrono::system_clock::now();
		auto timeMillis1 = std::chrono::duration_cast<std::chrono::milliseconds>(now1.time_since_epoch());
		long long timeMillisCount1 = timeMillis1.count();
		std::string Log = "Scan code ret = " + std::to_string(nRet) + "Scan Code time = " + std::to_string(timeMillisCount1 - timeMillisCount) + "\n";
		AppendLog(StringToLPCWSTR(Log));
		MV_CODEREADER_RESULT_BCR_EX2* stBcrResult = (MV_CODEREADER_RESULT_BCR_EX2*)stImageInfo.UnparsedBcrList.pstCodeListEx2;

		printf("Get One Frame: nChannelID[%d] Width[%d], Height[%d], nFrameNum[%d], nTriggerIndex[%d]\n",
			stImageInfo.nChannelID, stImageInfo.nWidth, stImageInfo.nHeight, stImageInfo.nFrameNum, stImageInfo.nTriggerIndex);
		printf("CodeNum[%d]\n", stBcrResult->nCodeNum);


		for (unsigned int i = 0; i < stBcrResult->nCodeNum; i++) {
			printf("Get CodeInfo: CodeNum[%d] CodeEx[%s]\n", i, stBcrResult->stBcrInfoEx2[i].chCode);
//			if (std::strlen(stBcrResult->stBcrInfoEx2[i].chCode) > 9) {
				codes.push_back(stBcrResult->stBcrInfoEx2[i].chCode); // 参数为 const char * 类型字符串
//			}
		}

		if (!codes.empty()) {
			break;
		}

		std::this_thread::sleep_for(std::chrono::microseconds((long long)m_exposureTime));
	}

	return 0;
}

int CodeReader::GetAcquisitionBurstFrameCount() 
{
	return m_acquisitionBurstFrameCount;
}

