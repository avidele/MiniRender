/*
 * @author: Avidel
 * @LastEditors: Avidel
 */

 #include "Utils/vulkan_util.hpp" // 包含新的应用程序和 Vulkan 工具类
 #include "spdlog/sinks/stdout_color_sinks.h"
 #include "spdlog/spdlog.h"
 #include <cstdlib>   // 为了 EXIT_SUCCESS 和 EXIT_FAILURE
 #include <exception> // 为了 std::exception
 
 int main(int /*argc*/, char* /*argv*/[]) {
     // 尽早设置日志记录器
     try {
         auto console = spdlog::stdout_color_mt("console");
         spdlog::set_default_logger(console);
         // 设置日志格式，包含文件名、行号和函数名
         spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%s:%# %!%$] [%^%l%$] %v");
         spdlog::set_level(spdlog::level::debug); // 设置日志级别为 debug
         spdlog::info("Logging initialized.");
     } catch (const spdlog::spdlog_ex& ex) {
         // 如果 spdlog 初始化失败，则使用标准输出
         printf("Log initialization failed: %s\n", ex.what());
         // 在这种早期阶段失败可能表明存在严重问题
     }
 
     // 创建并运行应用程序实例
     TriangleApplication app;
     try {
         app.run(); // 调用 run() 方法来启动初始化、主循环和清理
     } catch (const std::exception& e) {
         // 捕获并记录标准异常
         spdlog::critical("Application encountered an error: {}", e.what());
         return EXIT_FAILURE;
     } catch (...) {
         // 捕获所有其他类型的未知异常
         spdlog::critical("Application encountered an unknown error.");
         return EXIT_FAILURE;
     }
 
     spdlog::info("Application finished successfully.");
     return EXIT_SUCCESS; // 指示应用程序成功完成
 }