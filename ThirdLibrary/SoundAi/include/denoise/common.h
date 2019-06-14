/*
 *  Common interfaces for Audio Signal Processing
 *
 *  Copyright (C) 2019 SoundAI.com
 *  All Rights Reserved.
 */

#ifndef __SOUNDAI_ASP_COMMON_INTERFACE_H__
#define __SOUNDAI_ASP_COMMON_INTERFACE_H__

#include <stdint.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef SAI_API
#if defined(_WIN32) && defined(DLL_EXPORT)
#define SAI_API __declspec(dllexport)
#elif defined(_WIN32)
#define SAI_API __declspec(dllimport)
#elif defined(__GNUC__) && defined(DLL_EXPORT)
#define SAI_API __attribute__((visibility("default")))
#else
#define SAI_API
#endif
#endif

/**
 * Error codes.
 */
typedef enum {
  SAI_ASP_ERROR_SUCCESS = 0,
  SAI_ASP_ERROR_UNAUTHORIZED,
  SAI_ASP_ERROR_UNSUPPORTED,
  SAI_ASP_ERROR_INVALID_CONFIG,
  SAI_ASP_ERROR_INVALID_ENCODING,
  SAI_ASP_ERROR_UNKNOWN_ERROR
} sai_asp_err_t;

/**
 * Log levels.
 */
typedef enum {
  SAI_ASP_LOG_LEVEL_TRACE = 0,
  SAI_ASP_LOG_LEVEL_DEBUG,
  SAI_ASP_LOG_LEVEL_INFO,
  SAI_ASP_LOG_LEVEL_WARN,
  SAI_ASP_LOG_LEVEL_ERROR,
  SAI_ASP_LOG_LEVEL_CRITICAL,
  SAI_ASP_LOG_LEVEL_OFF
} sai_asp_log_level_t;

/**
 * Log handler.
 * Used to collect debug logs from denoise library.
 *
 * @param level Log level [TODO: missing log level definition]
 * @param tag [Optional] Log tag, usually module name.
 * @param func [Optional] Function (and line number if possible) where this log entry comes.
 * @param msg log message
 */
typedef void (*sai_logger_pt)(int level, const char *tag, const char *func, const char *msg);

#ifdef __cplusplus
}  // extern C
#endif

#endif /* __SOUNDAI_ASP_COMMON_INTERFACE_H__ */
