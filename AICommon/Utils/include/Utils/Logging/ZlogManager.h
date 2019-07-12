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

#ifndef __ZLOG_MANAGER_H_
#define __ZLOG_MANAGER_H_

// Zlog Ver-1.2.14
#include <zlog.h>
#include "Utils/Logging/Level.h"
namespace aisdk {
namespace utils {
namespace logging {

/**
 * A class used to save logs to files.
 */
class ZlogManager {
public:
    /**
     * Constructor.
     */
    ZlogManager();

    /**
     * Write logs message to zlog.
     */
	void zlog_put(Level level, const char *format, ...);

    /**
     * Destructor.
     */
    ~ZlogManager();
private:
    /// The current zlog category.
	zlog_category_t *m_cat;
};

}  // namespace logging
}  // namespace utils
}  // namespace aisdk

#endif  // __LOGGER_THREADMONIKER_H_
