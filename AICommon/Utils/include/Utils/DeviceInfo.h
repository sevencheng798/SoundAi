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
 
#ifndef __DEVICE_INFO_H_
#define __DEVICE_INFO_H_
#include <memory>
#include <mutex>
#include <unordered_set>
#include <Utils/NetworkStateObserverInterface.h>

namespace aisdk {
namespace utils {

class DeviceInfo {
public:
	/**
     * Create a DeviceInfo.
     *
     * @param configFile The restore info config file.
     * @return If successful, returns a new DeviceInfo, otherwise @c nullptr.
     */
	static std::unique_ptr<DeviceInfo> create(std::string &configFile);

	/**
     * Gets the Dialog Id.
     *
     * @return Dialog Id.
     */
    std::string getDialogId() const;

    /**
     * Gets the DSN.
     *
     * @return DSN.
     */
    std::string getDeviceSerialNumber() const;

	/**
	 * Gets current device execution environment. Acturally only for AIUI application scene.
	 * the scene include: deviceScene and testScene. 
	 * the prop key is: 'gm.domain.name'
	 * the prop value is:
	 * value of deviceScene - 'device.prod.nhf.cn'
	 * value of testScene - 'xushihong.nhf.cn'
	 *
	 * @return @c true if success, otherwise return @c false.
	 */
	 bool getDeviceScene();
	/**
	 * Check network state.
	 *
	 * @return @c true is connected, otherwise @c false.
	 */
	bool isConnected();

	/**
     * Adds an observer to be notified of Network state changes.
     *
     * @param observer The new observer to notify of Network state changes.
     */
    void addObserver(std::shared_ptr<NetworkStateObserverInterface> observer);

    /**
     * Removes an observer from the internal collection of observers synchronously. If the observer is not present,
     * nothing will happen.
     *
     * @param observer The observer to remove.
     */
    void removeObserver(std::shared_ptr<NetworkStateObserverInterface> observer);

	/**
	 * Notify network state be change.
	 */
	void notifyStateChange(NetworkStateObserverInterface::Status state);
	
    /**
     * Destructor.
     */
	~DeviceInfo();

private:
	/**
     * Constructor.
     */
	DeviceInfo(const std::string& dialogId, const std::string& deviceSerialNumber);

    /// Dialog ID
    std::string m_dialogId;

    /// DSN
    std::string m_deviceSerialNumber;

	/// The @c NetworkStateObserverInterface to notify Network state be changed.
    std::unordered_set<std::shared_ptr<NetworkStateObserverInterface>> m_observers;

	std::mutex m_mutex;
};
}	//utils
} // namespace aisdk
#endif //__DEVICE_INFO_H_

