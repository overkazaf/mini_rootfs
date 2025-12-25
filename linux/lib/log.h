#ifndef LOG_H
#define LOG_H

#include <stdio.h>

// 日志级别
typedef enum {
    LOG_LEVEL_DEBUG = 0,
    LOG_LEVEL_INFO,
    LOG_LEVEL_WARN,
    LOG_LEVEL_ERROR
} log_level_t;

// 初始化日志系统
void log_init(void);

// 设置日志级别
void log_set_level(log_level_t level);

// 获取时间戳字符串
const char* log_get_timestamp(void);

// 内部日志输出函数（带文件名和行号）
void log_output_ex(log_level_t level, const char* file, int line, const char* fmt, ...);

// 便捷宏 - 自动传入文件名和行号
#define LOG_DEBUG(fmt, ...) log_output_ex(LOG_LEVEL_DEBUG, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define LOG_INFO(fmt, ...)  log_output_ex(LOG_LEVEL_INFO,  __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define LOG_WARN(fmt, ...)  log_output_ex(LOG_LEVEL_WARN,  __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...) log_output_ex(LOG_LEVEL_ERROR, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

// 兼容旧代码的宏
#define LOG(fmt, ...)      LOG_INFO(fmt, ##__VA_ARGS__)

#endif // LOG_H
