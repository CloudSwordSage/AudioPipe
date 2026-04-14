/*
 * @Time    : 2026/4/14 21:43:59
 * @Author  : 墨烟行(GitHub UserName: CloudSwordSage)
 * @File    : logger.cpp
 * @License : GPL-3.0
 * @Desc    : 日志记录器实现
 */

#include "audio_pipe/logging/logger.hpp"

#include <iostream>
#include <iomanip>
#include <ctime>
#include <cstdint>

#ifdef _WIN32
#include <windows.h>
#endif

namespace audio_pipe {

    Logger::Logger() {
        // 读取环境变量 DEBUG=true
#ifdef _WIN32
        char buf[16] = {0};
        DWORD len = GetEnvironmentVariableA("DEBUG", buf, 16);
        if (len > 0) {
            std::string debug_env = buf;
            if (debug_env == "1" || debug_env == "true" ||
                debug_env == "TRUE") {
                debug_enabled_ = true;
            }
        }
#else
        const char * debug_env = getenv("DEBUG");
        if (debug_env && (std::string(debug_env) == "1" ||
                          std::string(debug_env) == "true")) {
            debug_enabled_ = true;
        }
#endif
    }

    Logger::~Logger() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (file_.is_open()) {
            file_.close();
        }
    }

    Logger & Logger::instance() {
        static Logger logger;
        return logger;
    }

    void Logger::init(const std::string & log_file) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (initialized_)
            return;

        if (!log_file.empty()) {
            file_.open(log_file, std::ios::out | std::ios::app);
        }
        initialized_ = true;
    }

    void Logger::debug(const std::string & flag, const std::string & msg) {
        if (!debug_enabled_)
            return; // 提前过滤，不进入后续逻辑
        log(Level::LOG_DEBUG, flag, msg);
    }

    void Logger::info(const std::string & flag, const std::string & msg) {
        log(Level::LOG_INFO, flag, msg);
    }

    void Logger::warn(const std::string & flag, const std::string & msg) {
        log(Level::LOG_WARN, flag, msg);
    }

    void Logger::error(const std::string & flag, const std::string & msg) {
        log(Level::LOG_ERROR, flag, msg);
    }

    void Logger::fatal(const std::string & flag, const std::string & msg) {
        log(Level::LOG_FATAL, flag, msg);
    }

    void Logger::log(
        Level level,
        const std::string & flag,
        const std::string & msg
    ) {
        std::lock_guard<std::mutex> lock(mutex_); // 线程安全

        std::string timestamp = get_timestamp();
        std::string level_str = level_to_str(level);

        // 格式：[时间] [LEVEL] [FLAG] MSG
        char line[2048];
        snprintf(
            line,
            sizeof(line),
            "[%s] [%s] [%s] %s",
            timestamp.c_str(),
            level_str.c_str(),
            flag.c_str(),
            msg.c_str()
        );

        // 输出到控制台
        std::cout << line << std::endl;

        // 输出到文件
        if (file_.is_open()) {
            file_ << line << std::endl;
        }
    }

    std::string Logger::level_to_str(Level level) {
        switch (level) {
            case Level::LOG_DEBUG:
                return "DEBUG";
            case Level::LOG_INFO:
                return "INFO";
            case Level::LOG_WARN:
                return "WARN";
            case Level::LOG_ERROR:
                return "ERROR";
            case Level::LOG_FATAL:
                return "FATAL";
            default:
                return "UNKNOWN";
        }
    }

    std::string Logger::get_timestamp() {
        using namespace std::chrono;
        auto now = system_clock::now();
        auto ms = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;

        std::time_t now_time = system_clock::to_time_t(now);
        std::tm local_tm{};

#ifdef _WIN32
        localtime_s(&local_tm, &now_time);
#else
        localtime_r(&now_time, &local_tm);
#endif

        char buf[64];
        snprintf(
            buf,
            sizeof(buf),
            "%04d/%02d/%02d %02d:%02d:%02d.%03d",
            local_tm.tm_year + 1900,
            local_tm.tm_mon + 1,
            local_tm.tm_mday,
            local_tm.tm_hour,
            local_tm.tm_min,
            local_tm.tm_sec,
            (int)ms.count()
        );
        return buf;
    }

} // namespace audio_pipe
