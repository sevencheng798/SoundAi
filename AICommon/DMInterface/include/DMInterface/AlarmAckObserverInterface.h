/*copyright 2019 gm its affiliates. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License").
 * You may not use this file except in compliance with the License.
 *
 * or in the "license" file accompanying this file. This file is distributed
 * on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
 * express or implied. See the License for the specific language governing
 * permissions and limitations under the License.
 */
 
#ifndef __ALARM_ACK_OBSERVER_INTERFACE_H_
#define __ALARM_ACK_OBSERVER_INTERFACE_H_

namespace aisdk {
namespace dmInterface {

class AlarmAckObserverInterface {
public:
	/**
     * This enum expresses the states that --
     */
    enum class Status {
        /// Waiting for the arrival of alarm clock time.
        WAITING,

        /// Another alarm clock is on the air.
        PENDING,

        /// Alarm clock time arrives and plays text. 
        PLAYING
    };
		
    /**
     * Destructor.
     */
    virtual ~AlarmAckObserverInterface() = default;


    /**
     * Called when the alarm ack state changes.
     *
     * @param status The current alarmack status.
     * @param reason The reason the status change occurred.
     */
    virtual void onAlarmAckStatusChanged(const Status newState, std::string ttsTxt) = 0;

    /**
     * This function converts the provided @c Status to a string.
     *
     * @param state The @c Status to convert to a string.
     * @return The string conversion of @c state.
     */	
	static std::string stateToString(Status status);
};


inline std::string AlarmAckObserverInterface::stateToString(Status status) {
	switch (status) {
		case AlarmAckObserverInterface::Status::WAITING:
		    return "WAITING";
		case AlarmAckObserverInterface::Status::PENDING:
		    return "PENDING";
		case AlarmAckObserverInterface::Status::PLAYING:
		    return "PLAYING";
	}
	return "Unknow state.";
}
/**
 * Write a @c AlarmAckObserverInterface::Status value to an @c ostream as a string.
 *
 * @param stream The stream to write the value to.
 * @param status The AlarmAckObserverInterface::Status value to write to the @c ostream as a string.
 * @return The @c ostream that was passed in and written to.
 */
inline std::ostream& operator<<(std::ostream& stream, const AlarmAckObserverInterface::Status &status) {
    return stream << AlarmAckObserverInterface::stateToString(status);
}

}	//dmInterface
} // namespace aisdk
#endif //__ALARM_ACK_OBSERVER_INTERFACE_H_


