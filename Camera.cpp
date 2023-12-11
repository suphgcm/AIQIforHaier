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

MV_CC_DEVICE_INFO* Camera::GetCameraByIpAddress() {
	if (m_isGot) {
		return m_mvDevInfo;
	}

	MV_CC_DEVICE_INFO_LIST stDeviceList;
	memset(&stDeviceList, 0, sizeof(MV_CC_DEVICE_INFO_LIST)); // 将数量和指针数组每个元素皆置 0

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

	// 创建句柄
	int nRet = MV_CC_CreateHandle(&m_handle, m_mvDevInfo);
	if (nRet != MV_OK) {
		printf("Create Handle fail! nRet [0x%x]\n", nRet);
		return false;
	}

	// 连接设备 todo: 考虑中间出现 false 时的析构
	nRet = MV_CC_OpenDevice(m_handle);
	if (nRet != MV_OK) {
		printf("Open Device fail! nRet [0x%x]\n", nRet);
		return false;
	}

	// 探测网络最佳包大小(只对GigE相机有效)
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

	// 增加数据包传输延迟，防止丢包
	nRet = MV_CC_SetIntValueEx(m_handle, "GevSCPD", 8000);
	if (nRet != MV_OK) {
		printf("Set GevSCPD fail! nRet [0x%x]\n", nRet);
	}

	// 设置关闭自动曝光
	nRet = MV_CC_SetEnumValue(m_handle, "ExposureAuto", 0);
	if (nRet != MV_OK) {
		printf("Set ExposureAuto fail! nRet [0x%x]\n", nRet);
	}

	// 设置曝光时间，设置曝光时间前请先关闭自动曝光，否则会设置失败
	nRet = MV_CC_SetFloatValue(m_handle, "ExposureTime", m_exposureTime);
	if (nRet != MV_OK) {
		printf("Set ExposureTime fail! nRet [0x%x]\n", nRet);
	}

	// 设置采集帧率
	nRet = MV_CC_SetFloatValue(m_handle, "AcquisitionFrameRate", m_acquisitionFrameRate);
	if (nRet != MV_OK) {
		printf("Set AcquisitionFrameRate fail! nRet [0x%x]\n", nRet);
	}

	// 让采集帧率和设置的帧率一样，如果不改为true，则会以最大带宽速率进行传输
	nRet = MV_CC_SetBoolValue(m_handle, "AcquisitionFrameRateEnable", true);
	if (nRet != MV_OK) {
		printf("Set AcquisitionFrameRateEnable fail! nRet [0x%x]\n", nRet);
	}

	// 设置自动曝光增益
	nRet = MV_CC_SetEnumValue(m_handle, "GainAuto", 0);
	if (nRet != MV_OK) {
		printf("Set GainAuto fail [%x]\n", nRet);
	}

	// 设置曝光增益，请先关闭自动曝光增益否则失败
	nRet = MV_CC_SetFloatValue(m_handle, "Gain", m_gain);
	if (nRet != MV_OK) {
		printf("Set Gain fail! nRet [0x%x]\n", nRet);
	}

	// 设置连续采集
	nRet = MV_CC_SetEnumValue(m_handle, "AcquisitionMode", 2);
	if (nRet != MV_OK) {
		printf("AcquisitionMode fail! nRet [0x%x]\n", nRet);
		exit(1);
	}

	// 触发模式关闭
	nRet = MV_CC_SetEnumValue(m_handle, "TriggerMode", 0);
	if (nRet != MV_OK) {
		printf("TriggerMode fail! nRet [0x%x]\n", nRet);
	}

	//// 设置触发源为软件触发
	//nRet = MV_CC_SetEnumValue(m_handle, "TriggerSource", 7);
	//if (nRet != MV_OK) {
	//	printf("TriggerSource fail! nRet [0x%x]\n", nRet);
	//}
	//// 设置触发帧计数
	//nRet = MV_CC_SetIntValue(m_handle, "AcquisitionBurstFrameCount", m_acquisitionBurstFrameCount);
	//if (nRet != MV_OK) {
	//	printf("Set AcquisitionBurstFrameCount fail! nRet [0x%x]\n", nRet);
	//}

	// 设置像素格式 BayerRG8
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

	// 关闭连接
	int nRet = MV_CC_CloseDevice(m_handle);
	if (nRet != MV_OK) {
		printf("ClosDevice fail! nRet [0x%x]\n", nRet);
		return false;
	}

	// 销毁句柄
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

	// 设置曝光时间
	int nRet = MV_CC_SetFloatValue(m_handle, "ExposureTime", exposureTime);
	if (nRet != MV_OK) {
		log_error("Camera code: " + e_deviceCode + ": Set exposure time failed! Ret=" + std::to_string(nRet));
		return false;
	}
	m_exposureTime = exposureTime;

	// 设置采集帧率
	nRet = MV_CC_SetFloatValue(m_handle, "AcquisitionFrameRate", acquisitionFrameRate);
	if (nRet != MV_OK) {
		log_error("Camera code: " + e_deviceCode + ": Set acquisition frame rate fail! Ret=" + std::to_string(nRet));
		return false;
	}
	m_acquisitionFrameRate = acquisitionFrameRate;

	// 设置曝光增益
	nRet = MV_CC_SetFloatValue(m_handle, "Gain", gain);
	if (nRet != MV_OK) {
		log_error("Camera code: " + e_deviceCode + ": Set gain fail! Ret=" + std::to_string(nRet));
		printf("Set Gain fail! nRet [0x%x]\n", nRet);
		return false;
	}
	m_gain = gain;

	// 采集帧计数
	m_acquisitionBurstFrameCount = acquisitionBurstFrameCount;

	// 图片压缩质量
	m_compressionQuality = compressionQuality;

	// 拍照间隔
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

//todo 临时修改 ，不停止 不开启 测试一下程序
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

bool Camera::GetImage(const std::string& path, void* args) {
	if (!m_isGrabbing) {
		return false;
	}
	//if (!CreateRecursiveDirectory(path)) {
	//	return false;
	//}
	for (int i = 0; i < m_acquisitionBurstFrameCount; ++i) {
		log_info("Camera code: " + e_deviceCode + ": Frame " + std::to_string(i) + " start!");
		MV_FRAME_OUT stOutFrame = { 0 };

		//// 打开软件触发
		//int nRet = MV_CC_SetCommandValue(m_handle, "TriggerSoftware");
		//if (MV_OK != nRet) {
		//	printf("Execute TriggerSoftware fail! nRet [0x%x]\n", nRet);
		//}
		auto now = std::chrono::system_clock::now();
		auto timeMillis = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch());
		long long timeMillisCount = timeMillis.count();

		int nRet = MV_CC_GetImageBuffer(m_handle, &stOutFrame, 1000);
		if (nRet != MV_OK) {
			wchar_t buffer[256];
			swprintf(buffer, 256, L"Get Image Buffer fail! nRet [0x%x]\n", nRet);
			MessageBox(NULL, buffer, L"CameraError", MB_OK);
			printf("Get Image Buffer fail! nRet [0x%x]\n", nRet);
			log_error("Camera code: " + e_deviceCode + ": Get image buffer failed! " + "Ret=" + std::to_string(nRet));
			return false;
		}
		auto now1 = std::chrono::system_clock::now();
		auto timeMillis1 = std::chrono::duration_cast<std::chrono::milliseconds>(now1.time_since_epoch());
		long long timeMillisCount1 = timeMillis1.count();

		printf("Get Image Buffer: Width[%d], Height[%d], FrameNum[%d]\n",
			stOutFrame.stFrameInfo.nWidth, stOutFrame.stFrameInfo.nHeight, stOutFrame.stFrameInfo.nFrameNum);

		// 转化图片格式为jpeg并保存在内存中,原始图片数据可能有14MB
		MV_SAVE_IMAGE_PARAM_EX3 to_jpeg;
		to_jpeg.pData = stOutFrame.pBufAddr;
		to_jpeg.nDataLen = stOutFrame.stFrameInfo.nFrameLen;
		to_jpeg.enPixelType = stOutFrame.stFrameInfo.enPixelType;
		to_jpeg.nWidth = stOutFrame.stFrameInfo.nWidth;
		to_jpeg.nHeight = stOutFrame.stFrameInfo.nHeight;
		// todo: 分配输出缓冲区，在上传线程中上传结束后会delete这一内存，5009408这个数值可能需要更改
		unsigned int bufferSize = 5009408;
		to_jpeg.pImageBuffer = new unsigned char[bufferSize];
		to_jpeg.nBufferSize = bufferSize;
		to_jpeg.enImageType = MV_Image_Jpeg;
		to_jpeg.nJpgQuality = m_compressionQuality; // 图片压缩质量，以后可能需要更改
		to_jpeg.iMethodValue = 2;

		nRet = MV_CC_SaveImageEx3(m_handle, &to_jpeg);
		if (nRet != MV_OK) {
			printf("MV_CC_SaveImageEx3 fail! nRet [0x%x]\n", nRet);
			delete[] to_jpeg.pImageBuffer; // 释放
			MV_CC_FreeImageBuffer(m_handle, &stOutFrame);
			log_error("Camera code: " + e_deviceCode + ": translate image format to jpeg failed! " + "Ret=" + std::to_string(nRet));
			return false;
		}
		auto now2 = std::chrono::system_clock::now();
		auto timeMillis2 = std::chrono::duration_cast<std::chrono::milliseconds>(now2.time_since_epoch());
		long long timeMillisCount2 = timeMillis2.count();

		ProcessUnit* unit = (ProcessUnit*)args;
		struct httpMsg msg;
		msg.pipelineCode = pipelineCode;
		msg.processesCode = unit->processesCode;
		msg.processesTemplateCode = unit->processesTemplateCode;
		msg.productSn = unit->productSn;
		msg.productSnCode = unit->productSnCode;
		msg.productSnModel = unit->productSnModel;
		msg.type = MSG_TYPE_PICTURE;
		msg.imageBuffer = to_jpeg.pImageBuffer;
		msg.imageLen = to_jpeg.nImageLen;
		Singleton::instance().push(msg);

		auto now3 = std::chrono::system_clock::now();
		auto timeMillis3 = std::chrono::duration_cast<std::chrono::milliseconds>(now3.time_since_epoch());
		long long timeMillisCount3 = timeMillis3.count();
		std::string Log = "Camera Code = " + e_deviceCode + ", GetImageBuffer Time = " + std::to_string(timeMillisCount1 - timeMillisCount) + "\n";
		AppendLog(StringToLPCWSTR(Log));
		log_info(Log);
		std::string Log1 = "Camera Code = " + e_deviceCode + ", Format to jpeg Time = " + std::to_string(timeMillisCount2 - timeMillisCount1) + "\n";
		AppendLog(StringToLPCWSTR(Log1));
		log_info(Log1);
		std::string Log2 = "Camera Code = " + e_deviceCode + ", Push message Time = " + std::to_string(timeMillisCount3 - timeMillisCount2) + "\n";
		AppendLog(StringToLPCWSTR(Log2));
		log_info(Log2);
		std::string Log3 = "Camera Code = "+ e_deviceCode +", camera interval = " + std::to_string(m_cameraInterval) + "\n";
		AppendLog(StringToLPCWSTR(Log3));

/*
		//将图片从内存保存到本地中
		MV_SAVE_IMAGE_TO_FILE_PARAM_EX file;
		file.nWidth = to_jpeg.nWidth;
		file.nHeight = to_jpeg.nHeight;
		file.enPixelType = PixelType_Gvsp_Jpeg;
		file.pData = to_jpeg.pImageBuffer;
		file.nDataLen = to_jpeg.nImageLen; // not initilized?

		file.enImageType = to_jpeg.enImageType;

		// file path
		std::string dir = path;
		std::replace(dir.begin(), dir.end(), '/', '\\');
		if (dir[dir.size() - 1] != '\\') {
			dir += '\\';
		}

		std::string serialNumber(reinterpret_cast<const char*>(m_mvDevInfo->SpecialInfo.stGigEInfo.chSerialNumber));

		auto now = std::chrono::system_clock::now();
		auto duration = now.time_since_epoch();
		auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
		std::string imageName = serialNumber + '_' + std::to_string(milliseconds) + ".jpeg";
		std::string imagePath = dir + imageName; // 建立目录
		file.pcImagePath = (char*)imagePath.c_str();

		file.nQuality = to_jpeg.nJpgQuality;
		file.iMethodValue = to_jpeg.iMethodValue;

		//将图片从内存保存到本地中
		nRet = MV_CC_SaveImageToFileEx(m_handle, &file);
		if (nRet != MV_OK) {
			printf("MV_CC_SaveImageToFileEx fail! nRet [0x%x]\n", nRet);
			delete[] to_jpeg.pImageBuffer;
			return false;
		}
*/
		// 释放
		//delete[] to_jpeg.pImageBuffer;
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

//void MyClass::SetFieldsByJson(const std::string& jsonString) {
//	using json = nlohmann::json;
//	using string = std::string;
//	json j = json::parse(jsonString);
//	string field;
//	if (j.contains("Field")) {
//		field = j["Field"];
//	}
//}