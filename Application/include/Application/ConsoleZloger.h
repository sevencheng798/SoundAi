/*
 * Copyright 2019 its affiliates. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License").
 * You may not use this file except in compliance with the License.
 *
 * or in the "license" file accompanying this file. This file is distributed
 * on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
 * express or implied. See the License for the specific language governing
 * permissions and limitations under the License.
 */

#ifndef __CONSOLE_ZLOGER_H_
#define __CONSOLE_ZLOGER_H_

#include <mutex>
#include <string>

#include <Utils/Logging/Logger.h>
#include <Utils/Logging/LogStringFormatter.h>
#include "Utils/Logging/ZlogManager.h"

namespace aisdk {
namespace application {

/**
 * A simple class that prints to the screen.
 */
class ConsoleZloger : public utils::logging::Logger {
public:
    /**
     * Constructor.
     */
    ConsoleZloger();

    void emit(
        utils::logging::Level level,
        std::chrono::system_clock::time_point time,
        const char* threadMoniker,
        const char* text) override;
private:
	/// Used to serialize access to @c m_logFormatter.
    std::mutex m_mutex;
	
	/// Object to format log strings correctly.
    utils::logging::LogStringFormatter m_logFormatter;

	/// Object to put log message to a save files.
	utils::logging::ZlogManager m_zlogManager;
};

}  // namespace application
}  // namespace aisdk

#endif  // __CONSOLE_ZLOGER_H_