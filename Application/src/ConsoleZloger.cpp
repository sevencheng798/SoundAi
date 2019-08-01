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
#include <iostream>
#include "Application/ConsoleZloger.h"

namespace aisdk {
namespace application {

ConsoleZloger::ConsoleZloger():
	utils::logging::Logger(utils::logging::Level::UNKNOWN) {

}

void ConsoleZloger::emit(
    utils::logging::Level level,
    std::chrono::system_clock::time_point time,
    const char* threadMoniker,
    const char* text) {
	std::lock_guard<std::mutex> lock(m_mutex);
	auto format = m_logFormatter.format(level, time, threadMoniker, text);
    std::cout << format << std::endl;
	m_zlogManager.zlog_put(level, format.c_str());
}


}  // namespace application
}  // namespace aisdk
