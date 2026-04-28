#include "../../include/logger.h"

#include <iostream>
#include <memory>
#include <vector>

#include "spdlog/async.h" 
#include "spdlog/sinks/basic_file_sink.h" 
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/spdlog.h" 


std::shared_ptr<spdlog::logger> Logger::logger_ = nullptr;

bool Logger::Init() {
    try {
        // 初始化 spdlog 的异步日志线程池。
        spdlog::init_thread_pool(8192, 1);

        // 创建一个终端日志输出 sink。
        auto console_sink = std::make_shared<
        spdlog::sinks::stdout_color_sink_mt>();

        // 创建一个文件日志输出sink
        auto file_sink = std::make_shared<
        spdlog::sinks::basic_file_sink_mt>("chat_room.log", true);

        //一个logger可以同时有多个sink
        std::vector<spdlog::sink_ptr> sinks{console_sink, file_sink};
         
        // 创建异步logger
        logger_ = std::make_shared<spdlog::async_logger>(
            "chat_room", //logger的名字
            sinks.begin(), // 指定这个logger使用哪些输出目标
            sinks.end(),
            spdlog::thread_pool(), // init_thread_pool()创建的线程池
            spdlog::async_overflow_policy::block); // 如果日志队列满了，当前线程会阻塞等待。
        
        // 把logger注册到spdlog的全局管理器中
        spdlog::register_logger(logger_);
        // 设置日志等级
        logger_->set_level(spdlog::level::debug);

        // 设置日志输出格式
        //%Y-%m-%d %H:%M:%S.%e 年-月-日 时-分-秒.毫秒;
        // %l 日志级别; 
        // %t 线程id; 
        // %v 真正的日志内容
        logger_->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] [thread%t] %v");

        // 设置自动刷新级别
        logger_->flush_on(spdlog::level::info);
        return true;
    } catch (const spdlog::spdlog_ex& ex) {
        std::cerr << "Logger init failed: " << ex.what() << '\n';
        return false;
    }
}

std::shared_ptr<spdlog::logger> Logger::Get() {

    // 使用方式：
    // Logger::Get()->info("server started");
    // Logger::Get()->error("send failed");
    return logger_;
}
