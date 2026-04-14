/*
 * @Time    : 2026/4/14 21:42:18
 * @Author  : 墨烟行(GitHub UserName: CloudSwordSage)
 * @File    : logger.hpp
 * @License : GPL-3.0
 * @Desc    : 日志记录器
 */

#pragma once

#include <string>
#include <mutex>
#include <fstream>
#include <chrono>

namespace audio_pipe {

    class Logger {
        public:
            // 获取单例
            static Logger & instance();

            // 日志等级
            enum class Level {
                LOG_DEBUG,
                LOG_INFO,
                LOG_WARN,
                LOG_ERROR,
                LOG_FATAL
            };

            // 初始化：设置日志文件路径（可选）
            void init(const std::string & log_file = "");

            // 对外日志接口
            void debug(const std::string & flag, const std::string & msg);
            void info(const std::string & flag, const std::string & msg);
            void warn(const std::string & flag, const std::string & msg);
            void error(const std::string & flag, const std::string & msg);
            void fatal(const std::string & flag, const std::string & msg);

            // 禁用拷贝
            Logger(const Logger &) = delete;
            Logger & operator=(const Logger &) = delete;

        private:
            Logger();
            ~Logger();

            // 内部真正写日志
            void log(
                Level level,
                const std::string & flag,
                const std::string & msg
            );

            // 等级转字符串
            static std::string level_to_str(Level level);

            // 获取带毫秒的时间字符串：2026/04/14 21:30:32.225
            static std::string get_timestamp();

        private:
            std::mutex mutex_;          // 线程安全锁
            std::ofstream file_;        // 日志文件
            bool debug_enabled_{false}; // DEBUG 是否启用（环境变量控制）
            bool initialized_{false};
    };

// 方便调用的全局宏（推荐使用）
#define LOG_DEBUG(flag, msg) audio_pipe::Logger::instance().debug(flag, msg)
#define LOG_INFO(flag, msg) audio_pipe::Logger::instance().info(flag, msg)
#define LOG_WARN(flag, msg) audio_pipe::Logger::instance().warn(flag, msg)
#define LOG_ERROR(flag, msg) audio_pipe::Logger::instance().error(flag, msg)
#define LOG_FATAL(flag, msg) audio_pipe::Logger::instance().fatal(flag, msg)

} // namespace audio_pipe
