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

#ifndef __LOGGER_SINK_MANAGER_H_
#define __LOGGER_SINK_MANAGER_H_

#include "Utils/Logging/Logger.h"

namespace aisdk {
namespace utils {
namespace logging {

/**
 * A manager to manage the sink logger and notify SinkObservers of any changes.
 */
class LoggerSinkManager {
public:
    /**
     * @return The one and only @c LoggerSinkManager instance.
     */
    static LoggerSinkManager& instance();

    /**
     * Add a SinkObserver to the manager.
     *
     * @param observer The @c SinkObserverInterface be be added.
     */
    void addSinkObserver(SinkObserverInterface* observer);

    /**
     * Remove a SinkObserver from the manager.
     *
     * @param observer The @c SinkObserverInterface to be removed.
     */
    void removeSinkObserver(SinkObserverInterface* observer);

    /**
     * Initialize the sink logger managed by the manager.
     * This function can be called only before any other threads in the process have been created by the
     * program.
     *
     * @param sink The new @c Logger to forward logs to.
     */
    void initialize(const std::shared_ptr<Logger>& sink);

private:
    /**
     * Constructor.
     */
    LoggerSinkManager();

    /// This mutex guards access to m_sinkObservers.
    std::mutex m_sinkObserverMutex;

    /// Vector of SinkObservers to be managed.
    std::vector<SinkObserverInterface*> m_sinkObservers;

    /// The @c Logger to forward logs to.
    std::shared_ptr<Logger> m_sink;
};


}  // namespace logging
}  // namespace utils
}  // namespace aisdk

#endif  // __LOGGER_SINK_MANAGER_H_
