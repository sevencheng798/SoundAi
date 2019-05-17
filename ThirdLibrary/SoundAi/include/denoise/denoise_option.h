/*
 *  Options for denoise
 *
 *  Copyright (C) 2019 SoundAI.com
 *  All Rights Reserved.
 */

/**
 * \@file denosie_option.h
 *
 * Modify this file to public denoise options
 *
 */

#ifndef __SOUNDAI_DENOISE_OPTION_H__
#define __SOUNDAI_DENOISE_OPTION_H__

#ifdef __cplusplus
extern "C" {
#endif

/** Authentication key, value is a string of auth_key */
extern const char *DENOISE_CFG_OPT_AUTH_KEY;

/** Enable log handler, value is @see sai_logger_pt */
extern const char *DENOISE_CFG_OPT_LOGGER;

/** Enable wake detection after denoise, value is @see sai_wake_cfg_t */
extern const char *DENOISE_CFG_OPT_WAKE_CFG;

/** Denoise optimized for ASR */
extern const char *DENOISE_DATA_TYPE_ASR;

/** Denoise optimized for VAD */
extern const char *DENOISE_DATA_TYPE_VAD;

/** Denoise optimized for VOIP */
extern const char *DENOISE_DATA_TYPE_VOIP;

/** Denoise event for local VAD */
extern const char *DENOISE_EVENT_TYPE_VAD;

/** Denoise event for wake detected, payload is `sai_denoise_wake_t` */
extern const char *DENOISE_EVENT_TYPE_WAKE;

/** Denoise event for hot word detected, payload is `sai_denoise_wake_t` */
extern const char *DENOISE_EVENT_TYPE_HOTWORD;

/** Denoise feature switch for voip */
extern const char *DENOISE_FEATURE_VOIP;

#ifdef __cplusplus
}  // extern C
#endif

#endif /* __SOUNDAI_DENOISE_OPTION_H__ */