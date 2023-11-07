#pragma once

#include "product.h"

// 引脚对应的process
class product2btest {
public:
	std::string pipelineCode;
	std::string pipelineName;
	std::string productName;
	std::string productSn;
	std::string productSnCode;
	std::string productSnModel;

public:
	int pinNumber;			//当前激活的trigger对应的gpio引脚号

	ProcessUnit* processUnitListHead;
	ProcessUnit* processUnitListFind;

	long shotTimestamp; // useless
	long lastTestTimestamp; // useless

public:
	product2btest() {};
	product2btest(int pin, std::string productSn, ProcessUnit* listHead, long timestamp)
		: pinNumber(pin), productSn(productSn), processUnitListHead(listHead),
		shotTimestamp(timestamp), lastTestTimestamp(timestamp) {
		//if (listHead != n)
		//productSnCode = productSn.substr(0, 9);
	}
	~product2btest() {};

	void SetProductInfo(std::string plCode, std::string plName, std::string pdName, std::string pdSnModel) {
		pipelineCode = plCode;
		pipelineName = plName;
		productName = pdName;
		productSnModel = pdSnModel;
	}
	ProcessUnit* GetTestList() {
		return processUnitListHead;
	}
	ProcessUnit* GetTestUnit() {
		return processUnitListFind;
	}
	ProcessUnit* GetNextTestUnit() {
		processUnitListFind = processUnitListFind->nextunit;
		return processUnitListFind;
	}

	long GetLastTestTimestamp() {
		return lastTestTimestamp;
	}
};

