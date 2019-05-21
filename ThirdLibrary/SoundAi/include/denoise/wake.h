/*
 *  Wake interfaces
 *
 *  Copyright (C) 2019 SoundAI.com
 *  All Rights Reserved.
 */

#ifndef __SOUNDAI_WAKE_INTERFACE_H__
#define __SOUNDAI_WAKE_INTERFACE_H__

#include "common.h"
#include "wake_option.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Wake config structure */
typedef struct sai_wake_cfg_t sai_wake_cfg_t;

/** Wake context structure */
typedef struct sai_wake_ctx_t sai_wake_ctx_t;

/**
 * Create config for wake module.
 * A single config instance could be used to initialize multiple wake contexts.
 *
 * @param[in] res_dir Root path for config and model files
 * @param[out] cfg Created config instance, NULL if failed
 * @return 0 if successful, otherwise @see sai_asp_err_t
 */
SAI_API int32_t sai_wake_cfg_init(const char *res_dir, sai_wake_cfg_t **cfg);

/**
 * Set config option.
 *
 * @param cfg Wake config instance
 * @param opt_name Option name
 * @param opt_value Option value
 * @return 0 if successful, otherwise @see sai_asp_err_t
 */
SAI_API int32_t sai_wake_cfg_set_option(sai_wake_cfg_t *cfg, const char *opt_name,
                                        const void *opt_value);

/**
 * Wake event handler
 *
 * @note the implementation should copy the data and return ASAP (< 1ms),
 * otherwise the worker thread would be blocked.
 *
 * @param ctx Wake context instance
 * @param word Text of wake/hot word
 * @param len_ms Time length of wake/hot word, in ms
 * @param data Samples of wake/hot word
 * @param size Total bytes of samples
 * @param user_data User-defined data
 */
typedef void (*sai_wake_event_handler_pt)(sai_wake_ctx_t *ctx, const char *word, int32_t len_ms,
                                          const char *data, size_t size, void *user_data);

/**
 * Add an event handler.
 * Multiple event handlers could be added.
 *
 * @param cfg Wake config instance
 * @param handler Wake event handler
 * @param user_data User-defined data
 * @return 0 if successful, otherwise @see sai_asp_err_t
 */
SAI_API int32_t sai_wake_cfg_add_event_handler(sai_wake_cfg_t *cfg,
                                               sai_wake_event_handler_pt handler, void *user_data);

/**
 * Destroy and release resources in config.
 *
 * @param cfg Wake config instance to be released.
 */
SAI_API void sai_wake_cfg_release(sai_wake_cfg_t *cfg);

/**
 * Create and initialize wake instance
 *
 * @param[in] cfg Wake config instance
 * @param[out] ctx Newly created wake context, NULL if failed
 * @return 0 if successful, otherwise @see sai_asp_err_t
 */

SAI_API int32_t sai_wake_init(const sai_wake_cfg_t *cfg, sai_wake_ctx_t **ctx);

/**
 * Process audio samples.
 * Once wake/hot word is detected, event handler will be invoked in worker thread.
 *
 * @param ctx Wake context instance
 * @param data Input samples
 * @param size Total bytes of input samples
 * @return 0 if successful, otherwise @see sai_asp_err_t
 */
SAI_API int32_t sai_wake_feed(sai_wake_ctx_t *ctx, const char *data, size_t size);

/**
 * Release all resources allocated for wake instance.
 *
 * @note No further callbacks would be invoked after calling this method.
 *
 * @param ctx Wake context
 */
SAI_API void sai_wake_release(sai_wake_ctx_t *ctx);

/**
 * Get wake version.
 *
 * @return Wake version.
 */
SAI_API const char *sai_wake_get_version();

#ifdef __cplusplus
}  // extern C
#endif

#endif /* __SOUNDAI_WAKE_INTERFACE_H__ */
