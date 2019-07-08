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
#ifndef __ASR_REFRESH_CONFIGURATION_H_
#define __ASR_REFRESH_CONFIGURATION_H_
#include <memory>
#include <unordered_set>
#include <mutex>
#include <DMInterface/AutomaticSpeechRecognizerUIDObserverInterface.h>

namespace aisdk {
namespace asr {

class ASRRefreshConfiguration {
public:
	
	/// Constructor
	ASRRefreshConfiguration() = default;

    /**
     * Adds an observer to be notified of UID configuration changes.
     *
     * @param observer The new observer to notify of UID configuration changes.
     */
    void addObserver(std::shared_ptr<dmInterface::AutomaticSpeechRecognizerUIDObserverInterface> observer);

    /**
     * Removes an observer from the internal collection of observers synchronously. If the observer is not present,
     * nothing will happen.
     *
     * @param observer The observer to remove.
     */
    void removeObserver(std::shared_ptr<dmInterface::AutomaticSpeechRecognizerUIDObserverInterface> observer);

	/// Notify uid be change.
	void notifyAsrRefresh(const std::string &uid, const std::string &appid, const std::string &key, const std::string &deviceId);
private:

	/// The message listener, which will receive uid messages sent from sai sdk.
	std::unordered_set<std::shared_ptr<dmInterface::AutomaticSpeechRecognizerUIDObserverInterface>> m_observers;

	/// Mutex to serialize access to m_observer.
	std::mutex m_mutex;
};

}	//asr
} // namespace aisdk
#endif // __ASR_REFRESH_CONFIGURATION_H_
