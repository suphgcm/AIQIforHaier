#pragma warning(disable:4996)
#include <fstream>
#include <iostream>
#include <cmath>
#include <windows.h>
#include <process.h>
#include "libsndfile/sndfile.h"
#include <string>
#include <vector>
#include "AudioEquipment.h"
#pragma comment(lib, "winmm.lib")

PaDeviceIndex AudioEquipment::GetASIODeviceByName(const std::string& deviceName) const {
	using namespace std;
	int numDevices = Pa_GetDeviceCount();
	if (numDevices < 0) {
		cerr << "ERROR: Pa_GetDeviceCount returned 0x" << hex << numDevices << dec << endl;
		PaError err = numDevices;
		return err;
	}
	cout << "Number of devices = " << numDevices << endl;

	for (int i = 0; i < numDevices; i++) {
		const PaDeviceInfo* deviceInfo = Pa_GetDeviceInfo(i);
		if (Pa_GetHostApiInfo(deviceInfo->hostApi)->type != paASIO) {
			cerr << "not asio device #" << i << endl;
			continue;
		}

		if (deviceInfo->name == deviceName) {
			return i;
		}
	}
	return -1;
}

int AudioEquipment::Init() {
	using namespace std;

	// PortAudio 初始化
	PaError err = Pa_Initialize(); // 只在主线程中执行一次
	if (err != paNoError) {
		Pa_Terminate();
		cerr << "ERROR: Pa_Initialize returned 0x" << hex << err << dec << endl;
		cerr << "Error message: " << Pa_GetErrorText(err) << endl;
		return err;
	}

	// 查找音频设备
	m_deviceIndex = GetASIODeviceByName(m_deviceName);
	if (m_deviceIndex < 0) {
		Pa_Terminate();
		cerr << "ERROR: GetASIODeviceByName(" << m_deviceName << ")" << endl;
		return m_deviceIndex;
	}
	cout << "Get device name = " << m_deviceName << ", index = " << m_deviceIndex << endl;

	return 0;
}

PaError AudioEquipment::Terminate() {
	PaError paError = Pa_Terminate();
	return paError;
}

int AudioEquipment::ReadFile(const std::string& fileName) {
	using namespace std;

	ifstream inputFile;
	inputFile.open(fileName, ios::binary); // todo: 处理异常
	filebuf* pbuf = inputFile.rdbuf();

	m_fileSize = pbuf->pubseekoff(0, ios::end, ios::in);
	m_fileBuffer = new char[m_fileSize + 1];
	pbuf->pubseekpos(0, ios::beg);
	pbuf->sgetn(m_fileBuffer, m_fileSize);
	m_fileBuffer[m_fileSize] = '\0'; // 空字符 '\0' 标识字符串结束

	inputFile.close();

	return 0;
}

unsigned AudioEquipment::NextPowerOf2(unsigned val) const {
	val--;
	val = (val >> 1) | val;
	val = (val >> 2) | val;
	val = (val >> 4) | val;
	val = (val >> 8) | val;
	val = (val >> 16) | val;
	return ++val;
}

static int recordCallback(const void* inputBuffer, void* outputBuffer, unsigned long framesPerBuffer, const PaStreamCallbackTimeInfo* timeInfo, PaStreamCallbackFlags statusFlags, void* userData) {
	AudioEquipment::paTestData* data = (AudioEquipment::paTestData*)userData;
	ring_buffer_size_t elementsWriteable = PaUtil_GetRingBufferWriteAvailable(&data->ringBuffer);
	ring_buffer_size_t elementsToWrite = min(elementsWriteable, (ring_buffer_size_t)(framesPerBuffer * AudioEquipment::CHANNEL_RECORD));
	const AudioEquipment::SAMPLE* rptr = (const AudioEquipment::SAMPLE*)inputBuffer;
	// const char* optr = (const char*) outputBuffer;
	data->frameIndex += PaUtil_WriteRingBuffer(&data->ringBuffer, rptr, elementsToWrite);

	//(void)outputBuffer; /* Prevent unused variable warnings. */
	if (data->outputFrameoffset + framesPerBuffer * (AudioEquipment::CHANNEL_COUNT * AudioEquipment::BIT_DEPTH / 8) < data->size) {
		memcpy(outputBuffer, data->audio + data->outputFrameoffset, framesPerBuffer * (AudioEquipment::CHANNEL_COUNT * AudioEquipment::BIT_DEPTH / 8));
		data->outputFrameoffset = data->outputFrameoffset + framesPerBuffer * (AudioEquipment::CHANNEL_COUNT * AudioEquipment::BIT_DEPTH / 8);
	}
	else {
		memset(outputBuffer, 0, framesPerBuffer * (AudioEquipment::CHANNEL_COUNT * AudioEquipment::BIT_DEPTH / 8));
	}

	(void)timeInfo;
	(void)statusFlags;
	(void)userData;

	return paContinue;
}

static int threadFunctionWriteToRawFile(void* ptr) {
	AudioEquipment::paTestData* pData = (AudioEquipment::paTestData*)ptr;

	/* Mark thread started */
	pData->threadSyncFlag = 0;

	while (1)
	{
		ring_buffer_size_t elementsInBuffer = PaUtil_GetRingBufferReadAvailable(&pData->ringBuffer);
		if ((elementsInBuffer >= pData->ringBuffer.bufferSize / AudioEquipment::NUM_WRITES_PER_BUFFER) ||
			pData->threadSyncFlag)
		{
			void* ptr[2] = { 0 };
			ring_buffer_size_t sizes[2] = { 0 };

			/* By using PaUtil_GetRingBufferReadRegions, we can read directly from the ring buffer */
			ring_buffer_size_t elementsRead = PaUtil_GetRingBufferReadRegions(&pData->ringBuffer, elementsInBuffer, ptr + 0, sizes + 0, ptr + 1, sizes + 1);
			if (elementsRead > 0)
			{
				int i;
				for (i = 0; i < 2 && ptr[i] != NULL; ++i)
				{
					fwrite(ptr[i], pData->ringBuffer.elementSizeBytes, sizes[i], pData->file);
				}
				PaUtil_AdvanceRingBufferReadIndex(&pData->ringBuffer, elementsRead);
			}

			if (pData->threadSyncFlag)
			{
				break;
			}
		}

		/* Sleep a little while... */
		Pa_Sleep(20);
	}

	pData->threadSyncFlag = 0;

	return 0;
}

PaError AudioEquipment::startThread(paTestData* pData, ThreadFunctionType fn) {
#ifdef _WIN32
	typedef unsigned(__stdcall* WinThreadFunctionType)(void*);
	pData->threadHandle = (void*)_beginthreadex(NULL, 0, (WinThreadFunctionType)fn, pData, CREATE_SUSPENDED, NULL);
	if (pData->threadHandle == NULL) return paUnanticipatedHostError;

	/* Set file thread to a little higher prio than normal */
	SetThreadPriority(pData->threadHandle, THREAD_PRIORITY_ABOVE_NORMAL);

	/* Start it up */
	pData->threadSyncFlag = 1;
	ResumeThread(pData->threadHandle);
#endif

	/* Wait for thread to startup */
	while (pData->threadSyncFlag) {
		Pa_Sleep(10);
	}

	return paNoError;
}

int AudioEquipment::stopThread(paTestData* pData)
{
	pData->threadSyncFlag = 1;
	/* Wait for thread to stop */
	while (pData->threadSyncFlag) {
		Pa_Sleep(10);
	}
#ifdef _WIN32
	CloseHandle(pData->threadHandle);
	pData->threadHandle = 0;
#endif

	return paNoError;
}

int AudioEquipment::PlayAndRecord(const std::string& fileName, int numSeconds) {
	using namespace std;
	// 检查音频文件
	if (m_fileBuffer == nullptr || m_fileSize == -1) {
		cerr << "The preset audio file was not read correctly." << endl;
		return -1;
	}
	// 检查音频设备
	if (m_deviceIndex == -1) {
		cerr << "Audio device was not found." << endl;
		return -1;
	}

	// 初始化环形缓冲区
	int numSamples = NextPowerOf2((unsigned)(SAMPLING_RATE * 1 * CHANNEL_COUNT));
	int numBytes = numSamples * sizeof(SAMPLE);
	paTestData data = { 0 };

	data.ringBufferData = (SAMPLE*)PaUtil_AllocateMemory(numBytes);
	if (data.ringBufferData == nullptr) {
		cerr << "Could not allocate ring buffer data." << endl;
		return -1;
	}
	if (PaUtil_InitializeRingBuffer(&data.ringBuffer, sizeof(SAMPLE), numSamples, data.ringBufferData) < 0) {
		cerr << "Failed to initialize ring buffer. Size is not power of 2 ??" << endl;
		return -1;
	}

	// 输出参数
	PaStreamParameters outputParameters;
	outputParameters.device = m_deviceIndex;
	outputParameters.channelCount = CHANNEL_COUNT; // MONO output
	outputParameters.sampleFormat = paInt16; // 16 bit point output
	outputParameters.suggestedLatency = Pa_GetDeviceInfo(outputParameters.device)->defaultLowOutputLatency;

	// Use an ASIO specific structure. WARNING - this is not portable.
	PaAsioStreamInfo asioOutputInfo;
	asioOutputInfo.size = sizeof(PaAsioStreamInfo);
	asioOutputInfo.hostApiType = paASIO;
	asioOutputInfo.version = 1;
	asioOutputInfo.flags = paAsioUseChannelSelectors;
	int outputChannelSelectors[1] = { 0 }; /*  use the frist (left) ASIO device channel */
	asioOutputInfo.channelSelectors = outputChannelSelectors;
	outputParameters.hostApiSpecificStreamInfo = &asioOutputInfo;

	// 输入参数
	PaStreamParameters inputParameters;
	inputParameters.device = m_deviceIndex;
	inputParameters.channelCount = CHANNEL_RECORD;       /* MONO output */
	inputParameters.sampleFormat = paInt16; /* 16 bit point output */
	inputParameters.suggestedLatency = Pa_GetDeviceInfo(inputParameters.device)->defaultLowOutputLatency;

	// Use an ASIO specific structure. WARNING - this is not portable.
	PaAsioStreamInfo asioInputInfo;
	asioInputInfo.size = sizeof(PaAsioStreamInfo);
	asioInputInfo.hostApiType = paASIO;
	asioInputInfo.version = 1;
	asioInputInfo.flags = paAsioUseChannelSelectors;
	int inputChannelSelectors[1] = { 0 };//, 1 }; // use the frist (left) ASIO device channel
	asioInputInfo.channelSelectors = inputChannelSelectors;
	inputParameters.hostApiSpecificStreamInfo = &asioInputInfo;

	data.outputTotalFrameIndex = m_fileSize / (CHANNEL_COUNT * BIT_DEPTH / 8);
	data.outputFrameoffset = 0;
	data.size = m_fileSize;
	data.audio = m_fileBuffer;

	// 打开音频流
	PaStream* stream = nullptr;
	PaError err = Pa_OpenStream(
		&stream,
		&inputParameters,
		&outputParameters,
		SAMPLING_RATE,
		NFRAMES_PER_BLOCK,
		paClipOff,      /* we won't output out of range samples so don't bother clipping them */
		recordCallback,
		&data);
	if (err != paNoError) {
		return err;
	}

	// 创建录制文件
	data.file = fopen(fileName.c_str(), "wb");
	if (data.file == 0) {
		return -1;
	}

	// 启动文件写入的线程
	err = startThread(&data, threadFunctionWriteToRawFile);
	if (err != paNoError) {
		return err;
	}

	// 启动音频流，同时播放和录音
	err = Pa_StartStream(stream);
	if (err != paNoError) {
		return err;
	}

	// 等待 numSeconds 秒
	int delayCntr = 0;
	while (delayCntr++ < numSeconds) {
		printf("index = %d\n", data.frameIndex);
		fflush(stdout);
		Pa_Sleep(1000);
	}

	// 停止流处理及清理工作
	err = Pa_StopStream(stream);
	if (err != paNoError) {
		return err;
	}

	err = Pa_CloseStream(stream);
	if (err != paNoError) {
		return err;
	}

	err = stopThread(&data);
	if (err != paNoError) {
		return err;
	}

	fclose(data.file);
	data.file = 0;
	if (data.ringBufferData) {       /* Sure it is NULL or valid. */
		PaUtil_FreeMemory(data.ringBufferData);
	}
	return 0;
}

int AudioEquipment::SeparateStereoChannels(
	const std::string& fileName, const std::string& leftFile, const std::string& rightFile
) const {
	// Open the PCM file
	FILE* fp = fopen(fileName.c_str(), "rb");
	if (fp == nullptr) {
		std::cout << "Failed to open the PCM file " << fileName << std::endl;
		return -1;
	}

	// Calculate byte_depth and bytes_per_sec
	int byte_depth = BIT_DEPTH / 8; // bytes per sample
	int bytes_per_sec = SAMPLING_RATE * CHANNEL_RECORD * byte_depth; // bytes per second 待测：使用16000Hz？

	// Define a buffer to store one second of data
	unsigned char* buffer = new unsigned char[bytes_per_sec];

	// Create two new PCM files to store the left and right channels
	FILE* fp_left = fopen(leftFile.c_str(), "wb");
	if (fp_left == nullptr) {
		std::cout << "Failed to create the left channel PCM file." << std::endl;
		return -1;
	}
	FILE* fp_right = fopen(rightFile.c_str(), "wb");
	if (fp_right == nullptr) {
		std::cout << "Failed to create the right channel PCM file." << std::endl;
		return -1;
	}

	// Loop until the end of file
	int ret = 0;
	while (!feof(fp)) {
		// Read one second of data (bytes_per_sec bytes)
		size_t n = fread(buffer, 1, bytes_per_sec, fp);
		if (n < bytes_per_sec) {
			// If the data is not enough, break the loop
			ret = -1;
			break;
		}

		// Split the buffer into left and right channels
		for (int i = 0; i < bytes_per_sec; i += byte_depth * CHANNEL_RECORD) {
			// Write the left channel data to the left PCM file
			fwrite(buffer + i, 1, byte_depth, fp_left);
			// Write the right channel data to the right PCM file
			fwrite(buffer + i + byte_depth, 1, byte_depth, fp_right);
		}
	}

	// Close the PCM files
	fclose(fp);
	fclose(fp_left);
	fclose(fp_right);
	delete[] buffer;

	return ret;
}

int AudioEquipment::To16k(const std::string& filename) const {
	std::string curFile = filename;

	std::string convFile = filename;
	size_t pos = filename.rfind(".pcm");

	if (pos != std::string::npos) {
		convFile.insert(pos, "_16");
	}

	// 打开输入文件
	SF_INFO in_info; // 输入文件信息结构体
	in_info.samplerate = 48000; // 设置输入采样率为48000hz
	in_info.channels = 1; // 设置输入声道数为1
	in_info.format = SF_FORMAT_RAW | SF_FORMAT_PCM_16; // 设置输入格式为raw和16位整数
	//SNDFILE* in_file = sf_open(curFile.c_str(), SFM_READ, &in_info);
	SNDFILE* in_file = sf_open(curFile.c_str(), SFM_READ, &in_info);

	if (in_file == NULL) { // 检查打开是否成功
		printf("无法打开输入文件: %s\n", sf_strerror(in_file));
		return -1;
	}

	// 检查输入文件是否为48000hz双声道
	//if (in_info.samplerate != 48000 || in_info.channels != 2)
	//{
	//    printf("输入文件不是48000hz双声道的音频\n");
	//    sf_close(in_file); // 关闭输入文件
	//    return -1;
	//}

	// 创建输出文件
	SF_INFO out_info; // 输出文件信息结构体
	out_info.samplerate = 16000; // 设置输出采样率为16000hz
	out_info.channels = in_info.channels; // 设置输出声道数与输入相同
	out_info.format = in_info.format; // 设置输出格式与输入相同
	SNDFILE* out_file = sf_open(convFile.c_str(), SFM_WRITE, &out_info);
	if (out_file == NULL) { // 检查创建是否成功
		printf("无法创建输出文件: %s\n", sf_strerror(out_file));
		sf_close(in_file); // 关闭输入文件
		return -1;
	}

	// 创建一个缓冲区，用于存储输入和输出数据
	int BUFFER_SIZE = 4096;
	float* buffer = (float*)malloc(BUFFER_SIZE * sizeof(float));

	// 计算每次读取和写入的帧数，根据不同的采样率进行调整
	int in_frames = BUFFER_SIZE / in_info.channels; // 输入帧数
	int out_frames = (int)(in_frames * (double)out_info.samplerate / in_info.samplerate); // 输出帧数

	while (true) { // 循环处理输入文件的数据
		int read_count = sf_readf_float(in_file, buffer, in_frames); // 从输入文件读取数据，返回实际读取的帧数
		if (read_count == 0) { // 如果读取到了文件末尾，跳出循环
			break;
		}
		if (read_count < in_frames) { // 如果读取到了最后一块数据，调整输出帧数
			out_frames = (int)(read_count * (double)out_info.samplerate / in_info.samplerate);
		}

		for (int i = 0; i < out_frames; i++) { // 遍历输出帧数
			for (int j = 0; j < out_info.channels; j++) { // 遍历输出声道数
				int index = i * out_info.channels + j; // 计算输出数据在缓冲区中的索引
				int in_index = (int)(i * (double)in_info.samplerate / out_info.samplerate) * in_info.channels + j; // 计算对应的输入数据在缓冲区中的索引
				buffer[index] = buffer[in_index]; // 将输入数据复制到输出数据
			}
		}

		sf_writef_float(out_file, buffer, out_frames); // 将输出数据写入输出文件
	}

	// 释放资源并关闭文件
	free(buffer);
	sf_close(in_file);
	sf_close(out_file);
}

int AudioEquipment::CutFile(const std::string& inFile, const std::string& outFile, int startSecond, int endSecond) 
{
	// Open the PCM file
	FILE* fp_in;
	fopen_s(&fp_in, inFile.c_str(), "rb");
	if (fp_in == nullptr) {
		std::cout << "Failed to open the PCM file " << inFile << std::endl;
		return -1;
	}

	FILE* fp_out;
	fopen_s(&fp_out, outFile.c_str(), "wb");
	if (fp_out == nullptr) {
		std::cout << "Failed to create the left channel PCM file." << std::endl;
		fclose(fp_in);
		return -1;
	}

	// Calculate byte_depth and bytes_per_sec
	int byte_depth = BIT_DEPTH / 8; // bytes per sample
	int bytes_per_sec = SAMPLING_RATE_16K * CHANNEL_RECORD * byte_depth; // bytes per second

	// Define a buffer to store one second of data
	unsigned char* buffer = new unsigned char[bytes_per_sec];

	// Loop until the end of file
	int ret = 0;
	int second = 0;
	while (!feof(fp_in)) {
		// Read one second of data (bytes_per_sec bytes)
		size_t n = fread(buffer, 1, bytes_per_sec, fp_in);
		if (n < bytes_per_sec) {
			// If the data is not enough, break the loop
			ret = -1;
			break;
		}
		second++;
		if ((second >= startSecond) && (second < endSecond)) {
			fwrite(buffer, 1, bytes_per_sec, fp_out);
		}
	}

	// Close the PCM files
	fclose(fp_in);
	fclose(fp_out);
	delete[] buffer;

	return ret;
}

void AudioEquipment::PlayAudio(WAVEFORMATEX* pFormat) {
	HWAVEOUT hWaveOut;
	WAVEHDR WaveOutHdr;

	waveOutOpen(&hWaveOut, WAVE_MAPPER, pFormat, 0L, 0L, WAVE_FORMAT_DIRECT);

	WaveOutHdr.lpData = m_fileBuffer;
	WaveOutHdr.dwBufferLength = m_fileSize;
	WaveOutHdr.dwBytesRecorded = 0;
	WaveOutHdr.dwUser = 0L;
	WaveOutHdr.dwFlags = 0L;
	WaveOutHdr.dwLoops = 1;

	waveOutPrepareHeader(hWaveOut, &WaveOutHdr, sizeof(WAVEHDR));

	waveOutWrite(hWaveOut, &WaveOutHdr, sizeof(WAVEHDR));

	while (waveOutUnprepareHeader(hWaveOut, &WaveOutHdr, sizeof(WAVEHDR)) == WAVERR_STILLPLAYING) {
		Sleep(100);
	}

	waveOutClose(hWaveOut);
}

int AudioEquipment::RecordAudio(WAVEFORMATEX* pFormat, int seconds, std::string recordFile) {
	WAVEHDR waveHeader;
	HWAVEIN hWaveIn;
	
	char* buffer = new char[pFormat->nAvgBytesPerSec * seconds];

	MMRESULT result = waveInOpen(&hWaveIn, WAVE_MAPPER, pFormat, 0L, 0L, WAVE_FORMAT_DIRECT);
	if (result)
	{
		printf("Failed to open waveform input device.\n");
		return -1;
	}

	waveHeader.lpData = buffer;
	waveHeader.dwBufferLength = sizeof(buffer);
	waveHeader.dwBytesRecorded = 0;
	waveHeader.dwUser = 0L;
	waveHeader.dwFlags = 0L;
	waveHeader.dwLoops = 0L;

	result = waveInPrepareHeader(hWaveIn, &waveHeader, sizeof(WAVEHDR));
	if (result)
	{
		printf("Failed to prepare waveform header.\n");
		return -1;
	}

	result = waveInAddBuffer(hWaveIn, &waveHeader, sizeof(WAVEHDR));
	if (result)
	{
		printf("Failed to add buffer.\n");
		return -1;
	}

	result = waveInStart(hWaveIn);
	if (result)
	{
		printf("Failed to start recording.\n");
		return -1;
	}

	// Record for RECORD_TIME milliseconds
	Sleep(seconds);

	// Open a .pcm file for writing
	std::ofstream outFile(recordFile.c_str(), std::ios::binary);

	// Write the buffer to the file
	outFile.write(buffer, waveHeader.dwBytesRecorded);

	outFile.close();

	waveInStop(hWaveIn);
	waveInUnprepareHeader(hWaveIn, &waveHeader, sizeof(WAVEHDR));
	waveInClose(hWaveIn);

	return 0;
}

