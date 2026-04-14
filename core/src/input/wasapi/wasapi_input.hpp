/*
 * @Time    : 2026/4/14 21:17:04
 * @Author  : 墨烟行(GitHub UserName: CloudSwordSage)
 * @File    : wasapi_input.hpp
 * @License : GPL-3.0
 * @Desc    : WASAPI 音频输入流接口
 */

#pragma once

#include "audio_pipe/input.hpp" // 引入抽象基类
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <deque>
#include <vector>

// WASAPI 实现类，继承跨平台抽象基类
class WasapiInputStream : public audio_pipe::AudioInputStream {
    public:
        WasapiInputStream(
            std::string device_id,
            uint32_t sample_rate,
            uint16_t bit_depth,
            uint16_t channels,
            uint32_t frame_size
        );

        ~WasapiInputStream() override;

        // 重写抽象接口
        void start() override;
        void stop() override;
        std::vector<uint8_t> get_frame(int timeout_ms = 100) override;

    private:
        // 私有成员
        std::string device_id_;
        uint32_t sample_rate_;
        uint16_t bit_depth_;
        uint32_t frame_size_;
        uint16_t channels_;
        uint16_t block_align_ = 0;

        std::atomic<bool> running_{false};
        std::thread capture_thread_;
        std::mutex mutex_;
        std::condition_variable cv_;
        std::deque<std::vector<uint8_t>> queue_;
        std::vector<uint8_t> pending_;
        std::string last_error_;
        bool start_completed_ = false;
        bool start_succeeded_ = false;
        const size_t max_queue_size_ = 64;

        void capture_loop();
        void push_audio_bytes(const uint8_t * data, size_t size);
};

// 导出 WASAPI 设备枚举函数（给工厂调用）
std::vector<audio_pipe::InputDeviceInfo> enumerate_input_devices();
