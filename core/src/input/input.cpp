/*
 * @Time    : 2026/4/14 21:16:45
 * @Author  : 墨烟行(GitHub UserName: CloudSwordSage)
 * @File    : input.cpp
 * @License : GPL-3.0
 * @Desc    : 音频输入流工厂集合
 */

#include "audio_pipe/input.hpp"

#ifdef _WIN32
// Windows 平台：包含 WASAPI 实现
#include "./wasapi/wasapi_input.hpp"
#endif

namespace audio_pipe {
    // 枚举设备：自动路由到平台实现
    std::vector<InputDeviceInfo> enumerate_input_devices() {
#ifdef _WIN32
        return ::enumerate_input_devices(); // WASAPI 枚举函数
#else
        throw std::runtime_error("Unsupported platform");
#endif
    }

    // 创建流：自动返回对应平台的实现类
    std::unique_ptr<AudioInputStream> create_input_stream(
        const std::string & device_id,
        uint32_t sample_rate,
        uint16_t bit_depth,
        uint16_t channels,
        uint32_t frame_size
    ) {
#ifdef _WIN32
        // Windows：返回 WASAPI 实现
        return std::make_unique<WasapiInputStream>(
            device_id,
            sample_rate,
            bit_depth,
            channels,
            frame_size
        );
#else
        // 其他平台：预留扩展
        throw std::runtime_error("Audio input not supported on this platform");
#endif
    }

} // namespace audio_pipe
