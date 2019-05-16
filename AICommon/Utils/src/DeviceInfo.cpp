/*
 * Copyright 2018 gm its affiliates. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License").
 * You may not use this file except in compliance with the License.
 * A copy of the License is located at
 *
 *
 * or in the "license" file accompanying this file. This file is distributed
 * on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
 * express or implied. See the License for the specific language governing
 * permissions and limitations under the License.
 */
#include <fstream>
#include <Utils/Logging/Logger.h>
#include "Utils/DeviceInfo.h"

/// String to identify log entries originating from this file. 
static const std::string TAG("DeviceInfo");

#define LX(event) aisdk::utils::logging::LogEntry(TAG, event)

namespace aisdk {
namespace utils {

static const std::string DEFAULT_CPUINFO{"/proc/cpuinfo"};
static const char DELIM = ':';
bool getProcCPUInfo(std::string &serial);

std::unique_ptr<DeviceInfo> DeviceInfo::create(std::string &configFile){
    std::string dialogId;
    std::string deviceSerialNumber;

	if(configFile.empty()){
		AISDK_ERROR(LX("CreateFailed").d("reason", "configFileNotFound"));
		return nullptr;
	}

	if(!getProcCPUInfo(deviceSerialNumber)) {
		AISDK_ERROR(LX("CreateFailed").d("reason", "cpuinfoNotFound"));
		return nullptr;
	}
	// The follow is a temporary definition. 
	dialogId = "123456789";
	AISDK_INFO(LX("Create").d("SerialNumber", deviceSerialNumber));
	std::unique_ptr<DeviceInfo> instance(new DeviceInfo(dialogId, deviceSerialNumber));

	return instance;
}

std::string DeviceInfo::getDialogId() const {
    return m_dialogId;
}

std::string DeviceInfo::getDeviceSerialNumber() const {
    return m_deviceSerialNumber;
}

DeviceInfo::~DeviceInfo(){}

DeviceInfo::DeviceInfo(const std::string& dialogId, const std::string& deviceSerialNumber)
	:m_dialogId{dialogId}, m_deviceSerialNumber{deviceSerialNumber}{

}

/// reflects the device setup credentials
bool getProcCPUInfo(std::string &serial) {
	auto key = std::string("Serial");
	std::ifstream fin(DEFAULT_CPUINFO);
	if(!fin.is_open()) {
		AISDK_ERROR(LX("getProcCPUInfoFailed").d("reason", "cpuinfoOpenedFailed"));
		return false;
	}

	while(!fin.eof()) {
		std::string line;
		getline(fin, line);
		auto pos = line.find(key);
		if(pos != std::string::npos) {
			pos = line.find_last_of(DELIM);
			pos += 1; // skip delim :
			serial = line.substr(pos);
			break;
		}
	}

	if(serial.empty()) 
		return false;

	return true;
}

}// utils
} // namespace aisdk
