#ifndef AUDIO_PIPE_AP_C_API_H
#define AUDIO_PIPE_AP_C_API_H

#include <stddef.h>
#include <stdint.h>

#if defined(_WIN32) || defined(__CYGWIN__)
#if defined(AUDIO_STATIC_DEFINE)
#define AUDIO_API
#elif defined(AUDIO_PIPE_BUILD_DLL)
#define AUDIO_API __declspec(dllexport)
#else
#define AUDIO_API __declspec(dllimport)
#endif
#else
#if defined(__GNUC__) && __GNUC__ >= 4
#define AUDIO_API __attribute__((visibility("default")))
#else
#define AUDIO_API
#endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef enum ap_status {
    AP_STATUS_OK = 0,
    AP_STATUS_INVALID_ARGUMENT = 1,
    AP_STATUS_OUT_OF_RANGE = 2,
    AP_STATUS_UNSUPPORTED = 3,
    AP_STATUS_RUNTIME_ERROR = 4,
    AP_STATUS_UNKNOWN_ERROR = 5
} ap_status;

typedef enum ap_log_level {
    AP_LOG_LEVEL_DEBUG = 0,
    AP_LOG_LEVEL_INFO = 1,
    AP_LOG_LEVEL_WARN = 2,
    AP_LOG_LEVEL_ERROR = 3,
    AP_LOG_LEVEL_FATAL = 4
} ap_log_level;

typedef struct ap_input_device_info {
    const char * device_id;
    const char * display_name;
    uint32_t sample_rate;
    uint16_t channels;
    uint16_t bit_depth;
} ap_input_device_info;

typedef struct ap_input_device_list ap_input_device_list;
typedef struct ap_audio_input_stream ap_audio_input_stream;
typedef struct ap_audio_frame ap_audio_frame;

/* Returns a thread-local error message produced by the most recent failed API call. */
AUDIO_API const char * ap_get_last_error(void);

/* Enumerates input devices and transfers ownership of the list handle to the caller. */
AUDIO_API ap_status ap_enumerate_input_devices(ap_input_device_list ** out_device_list);
AUDIO_API size_t ap_input_device_list_get_count(const ap_input_device_list * device_list);
AUDIO_API const ap_input_device_info * ap_input_device_list_get_item(
    const ap_input_device_list * device_list,
    size_t index
);
AUDIO_API void ap_input_device_list_destroy(ap_input_device_list * device_list);

/* Creates an input stream handle that wraps the internal C++ implementation. */
AUDIO_API ap_status ap_create_input_stream(
    const char * device_id,
    uint32_t sample_rate,
    uint16_t bit_depth,
    uint16_t channels,
    uint32_t frame_size,
    ap_audio_input_stream ** out_stream
);
AUDIO_API ap_status ap_audio_input_stream_start(ap_audio_input_stream * stream);
AUDIO_API ap_status ap_audio_input_stream_stop(ap_audio_input_stream * stream);
AUDIO_API ap_status ap_audio_input_stream_get_frame(
    ap_audio_input_stream * stream,
    int timeout_ms,
    ap_audio_frame ** out_frame
);
AUDIO_API void ap_audio_input_stream_destroy(ap_audio_input_stream * stream);

/* The returned frame view remains valid until ap_audio_frame_destroy() is called. */
AUDIO_API const uint8_t * ap_audio_frame_data(const ap_audio_frame * frame);
AUDIO_API size_t ap_audio_frame_size(const ap_audio_frame * frame);
AUDIO_API void ap_audio_frame_destroy(ap_audio_frame * frame);

/* Writes logs through the existing internal logger implementation. */
AUDIO_API ap_status ap_logger_init(const char * log_file_path);
AUDIO_API ap_status ap_logger_write(
    ap_log_level level,
    const char * tag,
    const char * message
);

#ifdef __cplusplus
}
#endif

#endif
