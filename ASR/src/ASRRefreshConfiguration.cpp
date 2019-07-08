/*
 * Copyright 2019 gm its affiliates. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License").
 * You may not use this file except in compliance with the License.
 *
 * or in the "license" file accompanying this file. This file is distributed
 * on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
 * express or implied. See the License for the specific language governing
 * permissions and limitations under the License.
 */
#include <Utils/Logging/Logger.h>
#include "ASR/ASRRefreshConfiguration.h"

namespace aisdk {
namespace asr {
/// String to identify log entries originating from this file.
static const std::string TAG("ASRRefreshConfiguration");

/**
 * Create a LogEntry using this file's TAG and the specified event string.
 *
 * @param The event string for this @c LogEntry.
 */
#define LX(event) utils::logging::LogEntry(TAG, event)

void ASRRefreshConfiguration::addObserver(
	std::shared_ptr<dmInterface::AutomaticSpeechRecognizerUIDObserverInterface> observer) {
    if (!observer) {
        AISDK_ERROR(LX("addObserverFailed").d("reason", "nullObserver"));
        return;
    }
	std::lock_guard<std::mutex> lock(m_mutex);
	m_observers.insert(observer);
}

void ASRRefreshConfiguration::removeObserver(
	std::shared_ptr<dmInterface::AutomaticSpeechRecognizerUIDObserverInterface> observer) {
    if (!observer) {
        AISDK_ERROR(LX("addObserverFailed").d("reason", "nullObserver"));
        return;
    }
	std::lock_guard<std::mutex> lock(m_mutex);
	m_observers.erase(observer);
}

/// Notify uid be change.
void ASRRefreshConfiguration::notifyAsrRefresh(
	const std::string &uid, const std::string &appid, const std::string &key, const std::string &deviceId) {
	std::lock_guard<std::mutex> lock(m_mutex);
	for(auto observer : m_observers) {
		if(observer)
			observer->asrRefreshConfiguration(uid, appid, key, deviceId);
	}
}

}	//asr
} // namespace aisdk
