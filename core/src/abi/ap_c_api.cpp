#include "exported/audio_pipe/ap_c_api.h"

#include "audio_pipe/input/input.hpp"
#include "audio_pipe/logging/logger.hpp"

#include <exception>
#include <memory>
#include <new>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

struct ap_input_device_list {
        std::vector<audio_pipe::InputDeviceInfo> devices;
        std::vector<ap_input_device_info> device_views;
};

struct ap_audio_input_stream {
        std::unique_ptr<audio_pipe::AudioInputStream> implementation;
};

struct ap_audio_frame {
        std::vector<uint8_t> data;
};

namespace {

    thread_local std::string g_last_error_message;

    void clear_last_error() {
        g_last_error_message.clear();
    }

    void set_last_error(std::string message) {
        g_last_error_message = std::move(message);
    }

    ap_status map_runtime_status(const std::string & message) {
        if (message.find("Unsupported platform") != std::string::npos ||
            message.find("not supported on this platform") !=
                std::string::npos) {
            return AP_STATUS_UNSUPPORTED;
        }

        return AP_STATUS_RUNTIME_ERROR;
    }

    void rebuild_device_views(ap_input_device_list & device_list) {
        device_list.device_views.clear();
        device_list.device_views.reserve(device_list.devices.size());

        for (const auto & device : device_list.devices) {
            ap_input_device_info view{};
            view.device_id = device.device_id.c_str();
            view.display_name = device.display_name.c_str();
            view.sample_rate = device.sample_rate;
            view.channels = device.channels;
            view.bit_depth = device.bit_depth;
            device_list.device_views.push_back(view);
        }
    }

    audio_pipe::Logger::Level to_cpp_log_level(ap_log_level level) {
        switch (level) {
            case AP_LOG_LEVEL_DEBUG:
                return audio_pipe::Logger::Level::LOG_DEBUG;
            case AP_LOG_LEVEL_INFO:
                return audio_pipe::Logger::Level::LOG_INFO;
            case AP_LOG_LEVEL_WARN:
                return audio_pipe::Logger::Level::LOG_WARN;
            case AP_LOG_LEVEL_ERROR:
                return audio_pipe::Logger::Level::LOG_ERROR;
            case AP_LOG_LEVEL_FATAL:
                return audio_pipe::Logger::Level::LOG_FATAL;
            default:
                throw std::invalid_argument("Invalid log level");
        }
    }

    template <typename Invocable>
    ap_status invoke_c_api(Invocable && invocable) {
        clear_last_error();

        try {
            invocable();
            return AP_STATUS_OK;
        } catch (const std::invalid_argument & ex) {
            set_last_error(ex.what());
            return AP_STATUS_INVALID_ARGUMENT;
        } catch (const std::out_of_range & ex) {
            set_last_error(ex.what());
            return AP_STATUS_OUT_OF_RANGE;
        } catch (const std::bad_alloc &) {
            set_last_error("Memory allocation failed");
            return AP_STATUS_RUNTIME_ERROR;
        } catch (const std::exception & ex) {
            set_last_error(ex.what());
            return map_runtime_status(ex.what());
        } catch (...) {
            set_last_error("Unknown exception");
            return AP_STATUS_UNKNOWN_ERROR;
        }
    }

} // namespace

extern "C" {

const char * ap_get_last_error(void) {
    return g_last_error_message.c_str();
}

ap_status ap_enumerate_input_devices(ap_input_device_list ** out_device_list) {
    if (out_device_list != nullptr) {
        *out_device_list = nullptr;
    }

    return invoke_c_api([&]() {
        if (out_device_list == nullptr) {
            throw std::invalid_argument("out_device_list must not be null");
        }

        std::unique_ptr<ap_input_device_list> device_list =
            std::make_unique<ap_input_device_list>();
        device_list->devices = audio_pipe::enumerate_input_devices();
        rebuild_device_views(*device_list);
        *out_device_list = device_list.release();
    });
}

size_t ap_input_device_list_get_count(
    const ap_input_device_list * device_list
) {
    clear_last_error();

    if (device_list == nullptr) {
        set_last_error("device_list must not be null");
        return 0;
    }

    return device_list->device_views.size();
}

const ap_input_device_info * ap_input_device_list_get_item(
    const ap_input_device_list * device_list,
    size_t index
) {
    clear_last_error();

    if (device_list == nullptr) {
        set_last_error("device_list must not be null");
        return nullptr;
    }

    if (index >= device_list->device_views.size()) {
        set_last_error("index is out of range");
        return nullptr;
    }

    return &device_list->device_views[index];
}

void ap_input_device_list_destroy(ap_input_device_list * device_list) {
    delete device_list;
}

ap_status ap_create_input_stream(
    const char * device_id,
    uint32_t sample_rate,
    uint16_t bit_depth,
    uint16_t channels,
    uint32_t frame_size,
    ap_audio_input_stream ** out_stream
) {
    if (out_stream != nullptr) {
        *out_stream = nullptr;
    }

    return invoke_c_api([&]() {
        if (out_stream == nullptr) {
            throw std::invalid_argument("out_stream must not be null");
        }

        std::unique_ptr<ap_audio_input_stream> stream_handle =
            std::make_unique<ap_audio_input_stream>();
        stream_handle->implementation = audio_pipe::create_input_stream(
            device_id != nullptr ? device_id : "",
            sample_rate,
            bit_depth,
            channels,
            frame_size
        );
        *out_stream = stream_handle.release();
    });
}

ap_status ap_audio_input_stream_start(ap_audio_input_stream * stream) {
    return invoke_c_api([&]() {
        if (stream == nullptr || !stream->implementation) {
            throw std::invalid_argument("stream must not be null");
        }

        stream->implementation->start();
    });
}

ap_status ap_audio_input_stream_stop(ap_audio_input_stream * stream) {
    return invoke_c_api([&]() {
        if (stream == nullptr || !stream->implementation) {
            throw std::invalid_argument("stream must not be null");
        }

        stream->implementation->stop();
    });
}

ap_status ap_audio_input_stream_get_frame(
    ap_audio_input_stream * stream,
    int timeout_ms,
    ap_audio_frame ** out_frame
) {
    if (out_frame != nullptr) {
        *out_frame = nullptr;
    }

    return invoke_c_api([&]() {
        if (stream == nullptr || !stream->implementation) {
            throw std::invalid_argument("stream must not be null");
        }

        if (out_frame == nullptr) {
            throw std::invalid_argument("out_frame must not be null");
        }

        std::unique_ptr<ap_audio_frame> frame =
            std::make_unique<ap_audio_frame>();
        frame->data = stream->implementation->get_frame(timeout_ms);
        *out_frame = frame.release();
    });
}

void ap_audio_input_stream_destroy(ap_audio_input_stream * stream) {
    delete stream;
}

const uint8_t * ap_audio_frame_data(const ap_audio_frame * frame) {
    clear_last_error();

    if (frame == nullptr) {
        set_last_error("frame must not be null");
        return nullptr;
    }

    return frame->data.data();
}

size_t ap_audio_frame_size(const ap_audio_frame * frame) {
    clear_last_error();

    if (frame == nullptr) {
        set_last_error("frame must not be null");
        return 0;
    }

    return frame->data.size();
}

void ap_audio_frame_destroy(ap_audio_frame * frame) {
    delete frame;
}

ap_status ap_logger_init(const char * log_file_path) {
    return invoke_c_api([&]() {
        audio_pipe::Logger::instance().init(
            log_file_path != nullptr ? log_file_path : ""
        );
    });
}

ap_status ap_logger_write(
    ap_log_level level,
    const char * tag,
    const char * message
) {
    return invoke_c_api([&]() {
        const audio_pipe::Logger::Level cpp_level = to_cpp_log_level(level);
        std::string tag_text = tag != nullptr ? tag : "";
        std::string message_text = message != nullptr ? message : "";

        switch (cpp_level) {
            case audio_pipe::Logger::Level::LOG_DEBUG:
                audio_pipe::Logger::instance().debug(tag_text, message_text);
                break;
            case audio_pipe::Logger::Level::LOG_INFO:
                audio_pipe::Logger::instance().info(tag_text, message_text);
                break;
            case audio_pipe::Logger::Level::LOG_WARN:
                audio_pipe::Logger::instance().warn(tag_text, message_text);
                break;
            case audio_pipe::Logger::Level::LOG_ERROR:
                audio_pipe::Logger::instance().error(tag_text, message_text);
                break;
            case audio_pipe::Logger::Level::LOG_FATAL:
                audio_pipe::Logger::instance().fatal(tag_text, message_text);
                break;
        }
    });
}

} // extern "C"
