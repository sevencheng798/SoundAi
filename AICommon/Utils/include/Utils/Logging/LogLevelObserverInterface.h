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

#ifndef __LOG_LEVEL_OBSERVER_INTERFACE_H_
#define __LOG_LEVEL_OBSERVER_INTERFACE_H_

#include <memory>

namespace aisdk {
namespace utils {
namespace logging {

// forward declaration
class Logger;

/**
 * This interface class allows notifications when the a Logger level be changed.
 */
class LogLevelObserverInterface {
public:
    /**
     * This function will be called when the logLevel changes.
     *
     * @param status The updated logLevel
     */
    virtual void onLogLevelChanged(Level level) = 0;

    /**
     * Destructor.
     */
    virtual ~LogLevelObserverInterface() = default;
};

}  // namespace logging
}  // namespace utils
}  // namespace aisdk

#endif  // __LOG_LEVEL_OBSERVER_INTERFACE_H_