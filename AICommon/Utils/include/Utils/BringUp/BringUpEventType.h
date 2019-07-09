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
 
#ifndef __BRINGUP_EVENT_TYPE_H_
#define __BRINGUP_EVENT_TYPE_H_

namespace aisdk {
namespace utils {
namespace bringup{

enum class eventType{
    BRINGUP_RESTART_CONFIG = 1,
    BRINGUP_START_BIND,
    BRINGUP_BIND_COMPLETE,
    BRINGUP_GMJK_START,
    MICROPHONE_OFF,
    MICROPHONE_ON,
    NET_DISCONNECTED,
    BRINGUP_UPGRADE_START,
    BRINGUP_PULSE_SCORE,
    ALARM_ACK
};


}   //bringup
}	//utils
} // namespace aisdk
#endif //__DEVICE_INFO_H_

