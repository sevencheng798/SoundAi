/*
 * Copyright 2019 gm its affiliates. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License").
 * You may not use this file except in compliance with the License.
 *
 *
 * or in the "license" file accompanying this file. This file is distributed
 * on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
 * express or implied. See the License for the specific language governing
 * permissions and limitations under the License.
 */

#ifndef __ECHO_CANCELLATION_OBSERVER_INTERFACE_H_
#define __ECHO_CANCELLATION_OBSERVER_INTERFACE_H_

namespace aisdk {
namespace kwd {

/**
 * A EchoCancellationObserverInterface is an interface class .
 */
class EchoCancellationObserverInterface {
public:
    /**
     * Destructor.
     */
    virtual ~EchoCancellationObserverInterface() = default;

    virtual void onEchoCancellationFrame(void *frame, size_t size) = 0;
};

}  // namespace dmInterface
}  // namespace aisdk

#endif  // __KEYWORD_OBSERVER_INTERFACE_H_