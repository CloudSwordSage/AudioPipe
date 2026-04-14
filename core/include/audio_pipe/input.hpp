/*
 * @Time    : 2026/4/14 21:12:46
 * @Author  : 墨烟行(GitHub UserName: CloudSwordSage)
 * @File    : input.hpp
 * @License : GPL-3.0
 * @Desc    : 音频输入流接口
 */

#pragma once

#include <vector>
#include <string>
#include <memory>
#include <cstdint>
#include <stdexcept>

namespace audio_pipe {

    // 设备信息结构体
    struct InputDeviceInfo {
            std::string device_id;    // 设备唯一ID
            std::string display_name; // 设备显示名称
            uint32_t sample_rate = 0; // 采样率
            uint16_t channels = 0;    // 通道数
            uint16_t bit_depth = 0;   // 位深
    };

    // 【抽象基类】音频输入流
    class AudioInputStream {
        public:
            virtual ~AudioInputStream() = default;

            // 启动采集
            virtual void start() = 0;
            // 停止采集
            virtual void stop() = 0;
            // 获取一帧音频数据（超时时间ms）
            virtual std::vector<uint8_t> get_frame(int timeout_ms = 100) = 0;

            // 禁用拷贝，只允许移动/智能指针
            AudioInputStream(const AudioInputStream &) = delete;
            AudioInputStream & operator=(const AudioInputStream &) = delete;

        protected:
            AudioInputStream() = default;
    };

    // ==================== 工厂函数 ====================
    // 枚举所有可用的音频输入设备
    std::vector<InputDeviceInfo> enumerate_input_devices();

    // 创建音频输入流（根据当前操作系统自动选择平台实现）
    // 参数：设备ID、采样率、位深、通道数、帧大小
    std::unique_ptr<AudioInputStream> create_input_stream(
        const std::string & device_id,
        uint32_t sample_rate,
        uint16_t bit_depth,
        uint16_t channels,
        uint32_t frame_size
    );

} // namespace audio_pipe
