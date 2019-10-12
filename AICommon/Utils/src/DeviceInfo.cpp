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

#include "properties.h"

/// String to identify log entries originating from this file. 
static const std::string TAG("DeviceInfo");

#define LX(event) aisdk::utils::logging::LogEntry(TAG, event)

namespace aisdk {
namespace utils {

/// Application scene key:value in the /data/default.prop.
static const char *keyScene = "gm.domain.name";
static const char *matchScene = "device.prod";

static const char *keyUtterance = "gm.utterance.save";

/// Wi-Fi state detect.
static const char *keyWifi = "net.wifi.state";

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

bool DeviceInfo::getDeviceScene() {
	char scene[64]={0};
	getprop((char *)keyScene, (char *)&scene);
	if(strstr(scene, matchScene) == NULL )
		return false;
	else 
		return true;
}

bool DeviceInfo::getUtteranceSave() {
	char state[4] = {0};
	getprop((char *)keyUtterance, (char *)&state);
	AISDK_DEBUG5(LX("getUtteranceSave").d("State", state));
	if(state[0] == '1') {
		return true;
	} else {
		return false;
	}
}

// wpa_state:
// INACTIVE
// COMPLETED
// DISCONNECTED
bool checkWireLessConnectState() {
	FILE* fp; 
	char match[] = "wpa_state=COMPLETED";
	char format[] = "wpa_cli -i wlan0 status | grep -r \"%s\"";
	char buf[32] = {0}; 
	char *command = NULL;
	int commandSize;
	bool ret = false;
	
	commandSize = strlen(format) + strlen(match);
	command = (char *)malloc(commandSize+1);
	if(!command) {
		AISDK_ERROR(LX("checkWireLessConnectStateFailed").d("reason", "askMemFailed"));
		return false;
	}
	memset(command, 0, commandSize);
	sprintf(command, format, match);

	if((fp = popen(command, "r")) != NULL){
		if((fgets(buf, 32, fp)) != NULL){
			AISDK_DEBUG0(LX("checkWireLessConnectState").d("STATE", buf));
			if(0 == memcmp(buf, match, strlen(match))) {
				ret = true;
			} else {
				ret = false;
			}
		}
		pclose(fp);
	}

	free(command);
	
	return ret;
}

bool DeviceInfo::isConnected() {
	char state[4]={0};
	getprop((char *)keyWifi, (char *)&state);
	AISDK_DEBUG0(LX("isConnected").d("networkState", state));
	
	if(state[0] == '1' ) {
    //if(state[0] == '1' || checkWireLessConnectState()) {
		notifyStateChange(NetworkStateObserverInterface::Status::CONNECTED);
		return true;
	}

	notifyStateChange(NetworkStateObserverInterface::Status::DISCONNECTED);
	return false;
}

void DeviceInfo::notifyStateChange(NetworkStateObserverInterface::Status state) {
	std::lock_guard<std::mutex> lock(m_mutex);
	for(auto observer : m_observers)
		observer->onNetworkStatusChanged(state);
}

void DeviceInfo::addObserver(std::shared_ptr<NetworkStateObserverInterface> observer) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_observers.insert(observer);
}

void DeviceInfo::removeObserver(std::shared_ptr<NetworkStateObserverInterface> observer) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_observers.erase(observer);
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
			pos += 2; // skip delim ': and SPACE'
			serial = line.substr(pos);
			break;
		}
	}
	// Close file stream.
	fin.close();
	if(serial.empty()) 
		return false;

	return true;
}

}// utils
} // namespace aisdk
