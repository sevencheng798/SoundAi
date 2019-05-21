/*
 *  Denoise interfaces
 *
 *  Copyright (C) 2019 SoundAI.com
 *  All Rights Reserved.
 */

#ifndef __SOUNDAI_DENOISE_INTERFACE_H__
#define __SOUNDAI_DENOISE_INTERFACE_H__

#include "common.h"
#include "denoise_option.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Denoise config structure */
typedef struct sai_denoise_cfg_t sai_denoise_cfg_t;

/** Denoise context structure */
typedef struct sai_denoise_ctx_t sai_denoise_ctx_t;

/**
 * Denoise wake event payload structure
 */
typedef struct {
  const char *word; /* Text of wake/hot word */
  const char *data; /* Samples of wake/hot word */
  size_t size; /* Total bytes of samples */
  float angle; /* DOA result */
  float score; /* Wake confidence */
} sai_denoise_wake_t;

/**
 * Create config for denoise modules.
 * A single config instance could be used to initialize multiple denoise contexts.
 *
 * @param[in] res_dir Root path for config and model files
 * @param[out] cfg Created config instance, NULL if failed
 * @return 0 if successful, otherwise @see sai_asp_err_t
 */
SAI_API int32_t sai_denoise_cfg_init(const char *res_dir,
                                     sai_denoise_cfg_t **cfg);

/**
 * Set config option.
 *
 * @param cfg Denoise config instance
 * @param opt_name Option name
 * @param opt_value Option value
 * @return 0 if successful, otherwise @see sai_asp_err_t
 */
SAI_API int32_t sai_denoise_cfg_set_option(sai_denoise_cfg_t *cfg,
                                           const char *opt_name,
                                           const void *opt_value);

/**
 * Denoise data handler
 *
 * @note the implementation should copy the data and return ASAP (< 1ms),
 * otherwise the worker thread would be blocked.
 *
 * @param ctx Denoise context instance
 * @param type Denoise type
 * @param data Samples of denoised data
 * @param size Total bytes of samples
 * @param user_data User-defined data
 */
typedef void (*sai_denoise_data_handler_pt)(sai_denoise_ctx_t *ctx,
                                            const char *type,
                                            const char *data,
                                            size_t size,
                                            void *user_data);

/**
 * Denoise event handler
 *
 * @note the implementation should return ASAP (< 1ms),
 * otherwise the worker thread would be blocked.
 *
 * @param ctx Denoise context instance
 * @param type Denoise event type
 * @param code Denoise event code
 * @param payload Denoise event payload instance
 * @param user_data User-defined data
 */
typedef void (*sai_denoise_event_handler_pt)(sai_denoise_ctx_t *ctx,
                                             const char *type,
                                             int32_t code,
                                             const void *payload,
                                             void *user_data);
/**
 * Add an data handler.
 * Multiple data handlers could be added.
 *
 * @param cfg Denoise config instance
 * @param type Denoise type
 * @param handler Denoise data handler
 * @param user_data User-defined data
 * @return 0 if successful, otherwise @see sai_asp_err_t
 */
SAI_API int32_t sai_denoise_cfg_add_data_handler(sai_denoise_cfg_t *cfg,
                                                  const char *type,
                                                  sai_denoise_data_handler_pt handler,
                                                  void *user_data);

/**
 * Add an event handler.
 * Multiple event handlers could be added.
 *
 * @param cfg Denoise config instance
 * @param type Denoise event type
 * @param handler Denoise event handler
 * @param user_data User-defined data
 * @return 0 if successful, otherwise @see sai_asp_err_t
 */
SAI_API int32_t sai_denoise_cfg_add_event_handler(sai_denoise_cfg_t *cfg,
                                                  const char *type,
                                                  sai_denoise_event_handler_pt handler,
                                                  void *user_data);

/**
 * Destroy and release resources in config.
 *
 * @param cfg Denoise config instance to be released.
 */
SAI_API void sai_denoise_cfg_release(sai_denoise_cfg_t *cfg);

/**
 * Create and initialize denoise instance
 *
 * @param[in] cfg Denoise config instance
 * @param[out] ctx Newly created denoise context, NULL if failed
 * @return 0 if successful, otherwise @see sai_asp_err_t
 */

SAI_API int32_t sai_denoise_init(const sai_denoise_cfg_t *cfg,
                                 sai_denoise_ctx_t **ctx);

/**
 * Process audio samples.
 * Data handler will be invoked in worker thread.
 *
 * @param ctx Denoise context instance
 * @param data Input samples
 * @param size Total bytes of input samples
 * @return 0 if successful, otherwise @see sai_asp_err_t
 */
SAI_API int32_t sai_denoise_feed(sai_denoise_ctx_t *ctx,
                                 const char *data,
                                 size_t size);

/**
 * Release all resources allocated for denoise instance.
 *
 * @note No further callbacks would be invoked after calling this method.
 *
 * @param ctx Denoise context
 */
SAI_API void sai_denoise_release(sai_denoise_ctx_t *ctx);

/**
 * Get estimated DOA angle
 *
 * @param[in] ctx Denoise context
 * @param[in] wake_len_ms Time length of wake/hot word, in ms
 * @param[in] delay_ms Calling delay since wake/hot word is detected, in ms
 * @param[out] angle Estimated DOA angle
 * @return 0 if successful, otherwise @see sai_asp_err_t
 */
SAI_API int32_t sai_denoise_get_doa(sai_denoise_ctx_t *ctx,
                                    int32_t wake_len_ms,
                                    int32_t delay_ms,
                                    float *angle);

/**
 * Start beaming
 *
 * @note If external wake engine is used, must be called to provide angle and
 * then `sai_denoise_data_handler_pt` will be called. If internal wake 
 * engine is used, this will be called automatically
 *
 * @param ctx Denoise context
 * @param angle Beam angle
 * @return 0 if successful, otherwise @see sai_asp_err_t
 */
SAI_API int32_t sai_denoise_start_beam(sai_denoise_ctx_t *ctx, float angle);

/**
 * Stop beaming
 *
 * @note Stop beamform to stop `sai_denoise_data_handler_pt`
 *
 * @param ctx Denoise context
 * @return 0 if successful, otherwise @see sai_asp_err_t
 */
SAI_API int32_t sai_denoise_stop_beam(sai_denoise_ctx_t *ctx);

/**
 * Enable or disable a feature
 *
 * @param ctx Denoise context
 * @param feature_name Feature name
 * @param flag Flag 0 disable the feature otherwise enable the feature
 * @return 0 if successful, otherwise @see sai_asp_err_t
 */
SAI_API int32_t sai_denoise_enable_feature(sai_denoise_ctx_t *ctx,
                                           const char *feature_name,
                                           int32_t flag);

/**
 * Set or update license key
 *
 * @param ctx Denoise context
 * @param license_key License key
 * @return 0 if successful, otherwise @see sai_asp_err_t
 */
SAI_API int32_t sai_denoise_set_license_key(sai_denoise_ctx_t *ctx,
                                            const char *license_key);
/**
 * Get denoise version.
 *
 * @return Denoise version.
 */
SAI_API const char *sai_denoise_get_version();

#ifdef __cplusplus
}
#endif

#endif /* __SOUNDAI_DENOISE_INTERFACE_H__ */
