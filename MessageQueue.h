#pragma once
#include <queue>
#include <mutex>
#include <condition_variable>

enum MSG_TYPE_E {
	MSG_TYPE_PICTURE,
	MSG_TYPE_TEXT
};

struct message {
	std::string pipelineCode;
	std::string processesCode;
	std::string processesTemplateCode;
	std::string productSn;
	std::string productSnCode;
	std::string productSnModel;
	double sampleTime;
	MSG_TYPE_E type;
	unsigned char* imageBuffer;
	unsigned int imageLen;
	std::string text;
};

class MessageQueue {
public:
	void push(const struct message& msg) {
		std::unique_lock<std::mutex> lock(_mutex);
		_queue.push(msg);
		_cv.notify_one();
	}
	void wait(struct message& msg) {
		std::unique_lock<std::mutex> lock(_mutex);
		while (!_queue.size()) _cv.wait(lock);
		msg = _queue.front();
		_queue.pop();
	}
	size_t size() {
		std::unique_lock<std::mutex> lock(_mutex);
		return _queue.size();
	}
private:
	std::mutex _mutex;
	std::queue<struct message> _queue;
	std::condition_variable _cv;
};

class Singleton {
public:
	static MessageQueue& instance()
	{
		static Singleton _singleton;
		return _singleton._mesque;
	}
private:
	Singleton() {};
	Singleton(const Singleton&);
	Singleton& operator=(const Singleton&);
	~Singleton() {};
	MessageQueue _mesque;
};