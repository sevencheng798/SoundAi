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
 
#ifndef __NETWORK_STATE_OBSERVER_INTERFACE_H_
#define __NETWORK_STATE_OBSERVER_INTERFACE_H_

namespace aisdk {
namespace utils {

class NetworkStateObserverInterface {
public:
	/**
     * This enum expresses the states that a logical NLP connection can be in.
     */
    enum class Status {
        /// NLP is not connected to Internet.
        DISCONNECTED,

        /// NLP is attempting to establish a connection to Internet.
        PENDING,

        /// NLP is connected to Internet.
        CONNECTED
    };
		
    /**
     * Destructor.
     */
    virtual ~NetworkStateObserverInterface() = default;


    /**
     * Called when the NLP connection state changes.
     *
     * @param status The current connection status.
     * @param reason The reason the status change occurred.
     */
    virtual void onNetworkStatusChanged(const Status newState) = 0;

    /**
     * This function converts the provided @c Status to a string.
     *
     * @param state The @c Status to convert to a string.
     * @return The string conversion of @c state.
     */	
	static std::string stateToString(Status status);
};


inline std::string NetworkStateObserverInterface::stateToString(Status status) {
	switch (status) {
		case NetworkStateObserverInterface::Status::DISCONNECTED:
		    return "DISCONNECTED";
		case NetworkStateObserverInterface::Status::PENDING:
		    return "PENDING";
		case NetworkStateObserverInterface::Status::CONNECTED:
		    return "CONNECTED";
	}
	return "Unknow state.";
}
/**
 * Write a @c NetworkStateObserverInterface::Status value to an @c ostream as a string.
 *
 * @param stream The stream to write the value to.
 * @param status The NetworkStateObserverInterface::Status value to write to the @c ostream as a string.
 * @return The @c ostream that was passed in and written to.
 */
inline std::ostream& operator<<(std::ostream& stream, const NetworkStateObserverInterface::Status &status) {
    return stream << NetworkStateObserverInterface::stateToString(status);
}

}	//utils
} // namespace aisdk
#endif //__NETWORK_STATE_OBSERVER_INTERFACE_H_

