#pragma once

#include "equnit.h"
#include <string>
#include "PortAudio/portaudio.h"
#include "PortAudio/pa_asio.h"
#include "PortAudio/pa_ringbuffer.h"
#include "PortAudio/pa_util.h"

// 声音播放和录制
class AudioEquipment :
	public equnit 
{
public:
	typedef short SAMPLE;
	struct paTestData {
		unsigned frameIndex;
		int threadSyncFlag;
		SAMPLE* ringBufferData;
		PaUtilRingBuffer ringBuffer;
		FILE* file;
		void* threadHandle;
		PaDeviceIndex useDevice;
		char* audio;
		int size;
		int outputTotalFrameIndex;
		int outputFrameoffset;
	};

	static const int SAMPLING_RATE = 48000;
	static const int CHANNEL_COUNT = 1;
	static const int CHANNEL_RECORD = 1;
	static const int BIT_DEPTH = 16;
	static const int NFRAMES_PER_BLOCK = 4096;
	static const int NUM_WRITES_PER_BUFFER = 4;

private:
	std::string m_deviceName = "Yamaha Steinberg USB ASIO";
	PaDeviceIndex m_deviceIndex = -1;
	char* m_fileBuffer = nullptr; // 音频文件
	long m_fileSize = -1;

	PaDeviceIndex GetASIODeviceByName(const std::string& deviceName) const;
	unsigned NextPowerOf2(unsigned val) const;

	typedef int (*ThreadFunctionType)(void*);
	PaError startThread(paTestData* pData, ThreadFunctionType fn);
	int stopThread(paTestData* pData);

public:
	AudioEquipment(std::string deviceTypeId, std::string deviceTypeName, std::string deviceTypeCode, std::string deviceCode, std::string deviceName) :
		equnit(deviceTypeId, deviceTypeName, deviceTypeCode, deviceCode, deviceName) {}
	~AudioEquipment() {
		if (m_fileBuffer != nullptr) {
			delete[] m_fileBuffer;
		}
	}

	int Init();
	PaError Terminate();

	int ReadFile(const std::string& fileName); // 读取预设音频文件

	int PlayAndRecord(const std::string& fileName, int numSeconds = 10); // todo: 加上要播放的参数
	int SeparateStereoChannels(const std::string& fileName, const std::string& leftFile, const std::string& rightFile) const;
	//int CropAudioFile(const std::string& input, const double startSeconds, const double endSeconds, const std::string& output) const;
	int To16k(const std::string& filename) const;
	int CutFile(const std::string& inFile, const std::string& outFile, int startSecond, int endSecond);
	// 顺序：Init -> record -> 48000_to_16000 -> seperate channel -> crop -> Terminate
};