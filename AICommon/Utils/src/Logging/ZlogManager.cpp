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
#include <string>
#include <sstream>

#include "Utils/Logging/ZlogManager.h"

namespace aisdk {
namespace utils {
namespace logging {

ZlogManager::ZlogManager() {
	int rc;

    rc = zlog_init("/cfg/log_all.conf");

    if (rc) {

        printf("init failed\n");
        return;

    }

    m_cat = zlog_get_category("my_aisdk");

    if (!m_cat) {

        printf("get cat fail\n");

        zlog_fini();
        return;
    }
}

void ZlogManager::zlog_put(Level level, const char *format, ...) {
	va_list args;
    int args1;
    va_start(args, format);
    args1 = va_arg(args, int);
    va_end(args);
	if(m_cat)
	zlog(m_cat, __FILE__, sizeof(__FILE__)-1, __func__, sizeof(__func__)-1, __LINE__, ZLOG_LEVEL_DEBUG, format, args1);
}

ZlogManager::~ZlogManager() {
	if(m_cat) {
		zlog_fini();
		m_cat = NULL;
	}
}

}  // namespace logging
}  // namespace utils
}  // namespace aisdk
