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

#ifndef __SINK_OBSERVER_INTERFACE_H_
#define __SINK_OBSERVER_INTERFACE_H_

#include <memory>

namespace aisdk {
namespace utils {
namespace logging {

// forward declaration
class Logger;

/**
 * This interface class allows notifications when the sink Logger changes.
 */
class SinkObserverInterface {
public:
    /**
     * This function will be called when the sink changes.
     *
     * @param sink The updated sink @c Logger
     */
    virtual void onSinkChanged(const std::shared_ptr<Logger>& sink) = 0;

    /**
     * Destructor.
     */
    virtual ~SinkObserverInterface() = default;
};

}  // namespace logging
}  // namespace utils
}  // namespace aisdk

#endif  // __SINK_OBSERVER_INTERFACE_H_