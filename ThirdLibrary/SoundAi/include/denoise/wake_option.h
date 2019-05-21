/*
 *  Options for wake
 *
 *  Copyright (C) 2019 SoundAI.com
 *  All Rights Reserved.
 */

/**
 * @file wake_option.h
 *
 * Modify this file to public wake options
 *
 */

#ifndef __SOUNDAI_ASP_OPTION_H__
#define __SOUNDAI_ASP_OPTION_H__

#ifdef __cplusplus
extern "C" {
#endif

/** Authentication key, value is a string of auth_key */
extern const char *WAKE_CFG_OPT_AUTH_KEY;

/** Enable log handler, value is @see sai_logger_pt */
extern const char *WAKE_CFG_OPT_LOGGER;

#ifdef __cplusplus
}  // extern C
#endif

#endif /* __SOUNDAI_ASP_OPTION_H__ */
