#include <mutex>
#include "Camera.h"
#include <string>
#include <algorithm>
#include <windows.h>
#include <cstdlib>
#include <chrono>
#include <thread>
#include "MessageQueue.h"
#include "product.h"
#include "Log.h"

extern void AppendLog(LPCWSTR text);
extern LPCWSTR StringToLPCWSTR(const std::string& s);
extern std::string pipelineCode;
extern struct counter {
	std::mutex mutex;
	long long count;
} Counter;

MV_CC_DEVICE_INFO* Camera::GetCameraByIpAddress() {
	if (m_isGot) {
		return m_mvDevInfo;
	}

	MV_CC_DEVICE_INFO_LIST stDeviceList;
	memset(&stDeviceList, 0, sizeof(MV_CC_DEVICE_INFO_LIST)); // ��������ָ������ÿ��Ԫ�ؽ��� 0

	int nRet = MV_CC_EnumDevices(MV_GIGE_DEVICE, &stDeviceList);
	if (nRet != MV_OK) {
		std::cerr << "Enum cameras failed! nRet [0x" << std::hex << nRet << std::dec << "]" << std::endl;
		return NULL;
	}
	if (stDeviceList.nDeviceNum <= 0) {
		std::cerr << "No cameras found!" << std::endl;
		return NULL;
	}

	for (unsigned int i = 0; i < stDeviceList.nDeviceNum; i++) {
		std::cout << "Camera[" << i << "]:" << std::endl;
		MV_CC_DEVICE_INFO* pstMVDevInfo = stDeviceList.pDeviceInfo[i];
		if (pstMVDevInfo == nullptr) {
			break;
		}

		if (pstMVDevInfo->nTLayerType == MV_GIGE_DEVICE) {
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
			if (ipAddress._Equal(IPADDR)) {
				m_mvDevInfo = pstMVDevInfo;
				m_isGot = true;
				return pstMVDevInfo;
			}

			//cameras.push_back(Camera(name, serialNumber, ipAddress, std::string(), pstMVDevInfo));
		}
		else if (pstMVDevInfo->nTLayerType == MV_USB_DEVICE) {
			std::cerr << "pstMVDevInfo->nTLayerType == MV_USB_DEVICE, wrong!" << std::endl;
		}
		else {
			std::cerr << "pstMVDevInfo->nTLayerType does not support." << std::endl;
		}
	}
	return NULL;
}

bool Camera::Init() {
	if (m_isInited) {
		return true;
	}
	if (!m_isGot) {
		std::cerr << "Camera is not got yet." << std::endl;
		return false;
	}

	// �������
	int nRet = MV_CC_CreateHandle(&m_handle, m_mvDevInfo);
	if (nRet != MV_OK) {
		printf("Create Handle fail! nRet [0x%x]\n", nRet);
		return false;
	}

	// �����豸 todo: �����м���� false ʱ������
	nRet = MV_CC_OpenDevice(m_handle);
	if (nRet != MV_OK) {
		printf("Open Device fail! nRet [0x%x]\n", nRet);
		return false;
	}

	// ̽��������Ѱ���С(ֻ��GigE�����Ч)
	if (m_mvDevInfo->nTLayerType == MV_GIGE_DEVICE) {
		int nPacketSize = MV_CC_GetOptimalPacketSize(m_handle);
		if (nPacketSize > 0) {
			nRet = MV_CC_SetIntValue(m_handle, "GevSCPSPacketSize", nPacketSize);
			if (nRet != MV_OK) {
				printf("Warning: Set Packet Size fail nRet [0x%x]!", nRet);
			}
		}
		else {
			printf("Warning: Get Packet Size fail nRet [0x%x]!", nPacketSize);
		}
	}

	// �������ݰ������ӳ٣���ֹ����
	nRet = MV_CC_SetIntValueEx(m_handle, "GevSCPD", 8000);
	if (nRet != MV_OK) {
		printf("Set GevSCPD fail! nRet [0x%x]\n", nRet);
	}

	// ���ùر��Զ��ع�
	nRet = MV_CC_SetEnumValue(m_handle, "ExposureAuto", 0);
	if (nRet != MV_OK) {
		printf("Set ExposureAuto fail! nRet [0x%x]\n", nRet);
	}

	// �����ع�ʱ�䣬�����ع�ʱ��ǰ���ȹر��Զ��ع⣬���������ʧ��
	nRet = MV_CC_SetFloatValue(m_handle, "ExposureTime", m_exposureTime);
	if (nRet != MV_OK) {
		printf("Set ExposureTime fail! nRet [0x%x]\n", nRet);
	}

	// ���òɼ�֡��
	nRet = MV_CC_SetFloatValue(m_handle, "AcquisitionFrameRate", m_acquisitionFrameRate);
	if (nRet != MV_OK) {
		printf("Set AcquisitionFrameRate fail! nRet [0x%x]\n", nRet);
	}

	// �òɼ�֡�ʺ����õ�֡��һ�����������Ϊtrue����������������ʽ��д���
	nRet = MV_CC_SetBoolValue(m_handle, "AcquisitionFrameRateEnable", true);
	if (nRet != MV_OK) {
		printf("Set AcquisitionFrameRateEnable fail! nRet [0x%x]\n", nRet);
	}

	// �����Զ��ع�����
	nRet = MV_CC_SetEnumValue(m_handle, "GainAuto", 0);
	if (nRet != MV_OK) {
		printf("Set GainAuto fail [%x]\n", nRet);
	}

	// �����ع����棬���ȹر��Զ��ع��������ʧ��
	nRet = MV_CC_SetFloatValue(m_handle, "Gain", m_gain);
	if (nRet != MV_OK) {
		printf("Set Gain fail! nRet [0x%x]\n", nRet);
	}

	// ���������ɼ�
	nRet = MV_CC_SetEnumValue(m_handle, "AcquisitionMode", 2);
	if (nRet != MV_OK) {
		printf("AcquisitionMode fail! nRet [0x%x]\n", nRet);
		exit(1);
	}

	// ����ģʽ�ر�
	nRet = MV_CC_SetEnumValue(m_handle, "TriggerMode", 0);
	if (nRet != MV_OK) {
		printf("TriggerMode fail! nRet [0x%x]\n", nRet);
	}

	//// ���ô���ԴΪ�������
	//nRet = MV_CC_SetEnumValue(m_handle, "TriggerSource", 7);
	//if (nRet != MV_OK) {
	//	printf("TriggerSource fail! nRet [0x%x]\n", nRet);
	//}
	//// ���ô���֡����
	//nRet = MV_CC_SetIntValue(m_handle, "AcquisitionBurstFrameCount", m_acquisitionBurstFrameCount);
	//if (nRet != MV_OK) {
	//	printf("Set AcquisitionBurstFrameCount fail! nRet [0x%x]\n", nRet);
	//}

	// �������ظ�ʽ BayerRG8
	nRet = MV_CC_SetEnumValue(m_handle, "PixelFormat", PixelType_Gvsp_BayerRG8);
	if (nRet != MV_OK) {
		printf("Set PixelFormat fail! nRet [0x%x]\n", nRet);
	}

	m_isInited = true;
	return true;
}

bool Camera::Destroy() {
	if (!m_isInited) {
		return true;
	}
	if (m_isGrabbing) {
		StopGrabbing();
	}

	// �ر�����
	int nRet = MV_CC_CloseDevice(m_handle);
	if (nRet != MV_OK) {
		printf("ClosDevice fail! nRet [0x%x]\n", nRet);
		return false;
	}

	// ���پ��
	nRet = MV_CC_DestroyHandle(m_handle);
	while (nRet != MV_OK) {
		printf("Destroy Handle fail! nRet [0x%x]\n", nRet);
		nRet = MV_CC_DestroyHandle(m_handle);
	}

	m_handle = nullptr;
	m_isInited = false;
	return true;
}

bool Camera::SetValuesForUninited(
	float exposureTime, float acquisitionFrameRate, float gain, int acquisitionBurstFrameCount, int compressionQuality, int cameraInterval
) {
	if (m_isInited) {
		return false;
	}

	m_exposureTime = exposureTime;
	m_acquisitionFrameRate = acquisitionFrameRate;
	m_gain = gain;
	m_acquisitionBurstFrameCount = acquisitionBurstFrameCount;
	m_compressionQuality = compressionQuality;
	m_cameraInterval = cameraInterval;

	return true;
}

bool Camera::SetValuesForInited(
	float exposureTime, float acquisitionFrameRate, float gain, int acquisitionBurstFrameCount, int compressionQuality, int cameraInterval
) {
	log_info("Camera code: " + e_deviceCode + ": Start set camera parameter!");
	if (!m_isInited) {
		return false;
	}

	// �����ع�ʱ��
	int nRet = MV_CC_SetFloatValue(m_handle, "ExposureTime", exposureTime);
	if (nRet != MV_OK) {
		log_error("Camera code: " + e_deviceCode + ": Set exposure time failed! Ret=" + std::to_string(nRet));
		return false;
	}
	m_exposureTime = exposureTime;
	log_info("Camera code: " + e_deviceCode + ": Set exposure time success!");
	// ���òɼ�֡��
	nRet = MV_CC_SetFloatValue(m_handle, "AcquisitionFrameRate", acquisitionFrameRate);
	if (nRet != MV_OK) {
		log_error("Camera code: " + e_deviceCode + ": Set acquisition frame rate fail! Ret=" + std::to_string(nRet));
		return false;
	}
	m_acquisitionFrameRate = acquisitionFrameRate;
	log_info("Camera code: " + e_deviceCode + ": Set acquisition frame rate success!");
	// �����ع�����
	nRet = MV_CC_SetFloatValue(m_handle, "Gain", gain);
	if (nRet != MV_OK) {
		log_error("Camera code: " + e_deviceCode + ": Set gain fail! Ret=" + std::to_string(nRet));
		printf("Set Gain fail! nRet [0x%x]\n", nRet);
		return false;
	}
	m_gain = gain;

	// �ɼ�֡����
	m_acquisitionBurstFrameCount = acquisitionBurstFrameCount;

	// ͼƬѹ������
	m_compressionQuality = compressionQuality;

	// ���ռ��
	m_cameraInterval = cameraInterval;
	log_info("Camera code: " + e_deviceCode + ": Success set camera config parameter");
	return true;
}

bool Camera::SetValuesByJson(const nlohmann::json& deviceParamConfigList) {
	float exposureTime = m_exposureTime;
	float acquisitionFrameRate = m_acquisitionFrameRate;
	float gain = m_gain;
	int acquisitionBurstFrameCount = m_acquisitionBurstFrameCount;
	int compressionQuality = m_compressionQuality;
	int cameraInterval = m_cameraInterval;

	for (auto deviceParam : deviceParamConfigList) {
		switch (CameraParammap[deviceParam["paramCode"]]) {
		case 0:
			acquisitionFrameRate = std::stof((std::string)deviceParam["paramValue"]);
			break;
		case 1:
			exposureTime = std::stof((std::string)deviceParam["paramValue"]);
			break;
		case 2:
			gain = std::stof((std::string)deviceParam["paramValue"]);
			break;
		case 3:
			// devicelatency = deviceParam["paramValue"];
			break;
		case 4:
			acquisitionBurstFrameCount = std::stoi((std::string)deviceParam["paramValue"]);
			break;
		case 5:
			compressionQuality = std::stoi((std::string)deviceParam["paramValue"]);
			break;
		case 6:
			cameraInterval = std::stoi((std::string)deviceParam["paramValue"]);
			break;
		default:
			break;
		}
	}

	if (m_isInited) {
		return SetValuesForInited(exposureTime, acquisitionFrameRate, gain, acquisitionBurstFrameCount, compressionQuality, cameraInterval);
	}
	return SetValuesForUninited(exposureTime, acquisitionFrameRate, gain, acquisitionBurstFrameCount, compressionQuality, cameraInterval);
}

bool Camera::StartGrabbing() {
	if (m_isGrabbing) {
		return true;
	}
	if (!m_isInited) {
		std::cerr << "Camera is not inited yet." << std::endl;
		return false;
	}

	int nRet = MV_CC_StartGrabbing(m_handle);
	if (nRet != MV_OK) {
		printf("Start Grabbing fail! nRet [0x%x]\n", nRet);
		return false;
	}

	m_isGrabbing = true;
	return true;
}

//todo ��ʱ�޸� ����ֹͣ ������ ����һ�³���
bool Camera::StopGrabbing() {
	//add by hwqi, never stop grabbing
	return true;

	if (!m_isGrabbing) {
		return true;
	}

	int nRet = MV_CC_StopGrabbing(m_handle);
	if (nRet != MV_OK) {
		printf("Stop Grabbing fail! nRet [0x%x]\n", nRet);
		return false;
	}

	m_isGrabbing = false;
	return true;
}

void Camera::Lock() {
	m_mutex.lock();
}
void Camera::UnLock() {
	m_mutex.unlock();
}

extern std::string projDir;

bool Camera::GetImage(const std::string& path, void* args) {
	if (!m_isGrabbing) {
		return false;
	}

	auto now = std::chrono::system_clock::now();
	auto duration = now.time_since_epoch();
	auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();

	ProcessUnit* unit = (ProcessUnit*)args;

	std::string PicturesPath = projDir.c_str();
	PicturesPath.append("\\pictures\\" + std::to_string(milliseconds));
	std::filesystem::create_directories(PicturesPath);

	for (int i = 0; i < m_acquisitionBurstFrameCount; ++i) {
		log_info("Camera code: " + e_deviceCode + ": Frame " + std::to_string(i) + " start!");
		MV_FRAME_OUT stOutFrame = { 0 };

		int nRet = MV_CC_GetImageBuffer(m_handle, &stOutFrame, 1000);
		if (nRet != MV_OK) {
			wchar_t buffer[256];
			swprintf(buffer, 256, L"Get Image Buffer fail! nRet [0x%x]\n", nRet);
			//MessageBox(NULL, buffer, L"CameraError", MB_OK);
			printf("Get Image Buffer fail! nRet [0x%x]\n", nRet);
			log_error("Camera code: " + e_deviceCode + ": Get image buffer failed! " + "Ret=" + std::to_string(nRet));
			return false;
		}

		printf("Get Image Buffer: Width[%d], Height[%d], FrameNum[%d]\n",
			stOutFrame.stFrameInfo.nWidth, stOutFrame.stFrameInfo.nHeight, stOutFrame.stFrameInfo.nFrameNum);

		// ת��ͼƬ��ʽΪjpeg���������ڴ���,ԭʼͼƬ���ݿ�����14MB
		MV_SAVE_IMAGE_PARAM_EX3 to_jpeg;
		to_jpeg.pData = stOutFrame.pBufAddr;
		to_jpeg.nDataLen = stOutFrame.stFrameInfo.nFrameLen;
		to_jpeg.enPixelType = stOutFrame.stFrameInfo.enPixelType;
		to_jpeg.nWidth = stOutFrame.stFrameInfo.nWidth;
		to_jpeg.nHeight = stOutFrame.stFrameInfo.nHeight;
		// todo: ������������������ϴ��߳����ϴ��������delete��һ�ڴ棬5009408�����ֵ������Ҫ����
		unsigned int bufferSize = 5009408;
		to_jpeg.pImageBuffer = new unsigned char[bufferSize];
		to_jpeg.nBufferSize = bufferSize;
		to_jpeg.enImageType = MV_Image_Jpeg;
		to_jpeg.nJpgQuality = m_compressionQuality; // ͼƬѹ���������Ժ������Ҫ����
		to_jpeg.iMethodValue = 2;

		nRet = MV_CC_SaveImageEx3(m_handle, &to_jpeg);
		if (nRet != MV_OK) {
			printf("MV_CC_SaveImageEx3 fail! nRet [0x%x]\n", nRet);
			delete[] to_jpeg.pImageBuffer; // �ͷ�
			MV_CC_FreeImageBuffer(m_handle, &stOutFrame);
			log_error("Camera code: " + e_deviceCode + ": translate image format to jpeg failed! " + "Ret=" + std::to_string(nRet));
			return false;
		}

		now = std::chrono::system_clock::now();
		duration = now.time_since_epoch();
		milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();

/*
		std::string filePath = projDir.c_str();
		filePath.append("\\" + pipelineCode + "\\" + unit->productSn + "\\" + unit->processesCode + "\\" + std::to_string(milliseconds) + ".jpeg");
		std::filesystem::create_directories(filePath.substr(0, filePath.find_last_of('\\')));
*/
		std::string filePath = PicturesPath;
		filePath.append("\\" + unit->productSn + "-" + unit->processesCode + "-" + e_deviceCode + "-" + std::to_string(milliseconds) + ".jpeg");
	
		FILE* fp = nullptr;
		fopen_s(&fp, filePath.c_str(), "wb");
		if (fp == nullptr) {
			std::cout << "Open file failed!" << std::endl;
		}
		else {
			fwrite(to_jpeg.pImageBuffer, 1, to_jpeg.nImageLen, fp);
			fclose(fp);
		}

		// �ͷ�
		delete[] to_jpeg.pImageBuffer;
		nRet = MV_CC_FreeImageBuffer(m_handle, &stOutFrame);
		if (nRet != MV_OK) {
			printf("Free Image Buffer fail! nRet [0x%x]\n", nRet);
			log_error("Camera code: " + e_deviceCode + ": Free image buffer failed! " + "Ret=" + std::to_string(nRet));
		}

		log_info("Camera code: " + e_deviceCode + ": Frame " + std::to_string(i) + " end!");

		std::this_thread::sleep_for(std::chrono::milliseconds(m_cameraInterval));
	}

	return true;
}

bool Camera::GetImageX() {
	if (!m_isGrabbing) {
		return false;
	}

	MV_FRAME_OUT stOutFrame = { 0 };

	int nRet = MV_CC_GetImageBuffer(m_handle, &stOutFrame, 1000);
	if (nRet != MV_OK) {
		printf("Get Image Buffer fail! nRet [0x%x]\n", nRet);
		return false;
	}

	MV_CC_FreeImageBuffer(m_handle, &stOutFrame);

	return true;
}

//void MyClass::SetFieldsByJson(const std::string& jsonString) {
//	using json = nlohmann::json;
//	using string = std::string;
//	json j = json::parse(jsonString);
//	string field;
//	if (j.contains("Field")) {
//		field = j["Field"];
//	}
//}