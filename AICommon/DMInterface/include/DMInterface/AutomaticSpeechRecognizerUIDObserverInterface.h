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

#ifndef __AUTOMATIC_SPEECH_RECOGNIZER_UID_OBSERVER_INTERFACE_H_
#define __AUTOMATIC_SPEECH_RECOGNIZER_UID_OBSERVER_INTERFACE_H_

#include <string>

namespace aisdk {
namespace dmInterface {

/**
 * Use @c name to intended for identifying sub-types of @c NLPDomain
 */
class AutomaticSpeechRecognizerUIDObserverInterface {
public:
    /**
     * Destructor.
     */
    ~AutomaticSpeechRecognizerUIDObserverInterface() = default;

	virtual void asrRefreshConfiguration(
		const std::string &appid, 
		const std::string &key,
		const std::string &uid) = 0;
};

}  // namespace dmInterface
}  // namespace aisdk

#endif  // __AUTOMATIC_SPEECH_RECOGNIZER_UID_OBSERVER_INTERFACE_H_