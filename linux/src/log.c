#include "log.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

// 起始时间（用于计算相对时间）
static struct timeval g_start_time = {0};
static int g_time_initialized = 0;
static log_level_t g_log_level = LOG_LEVEL_DEBUG;

// 日志级别名称
static const char* level_names[] = {
    "DEBUG",
    "INFO ",
    "WARN ",
    "ERROR"
};

// 日志级别颜色（ANSI）
static const char* level_colors[] = {
    "\033[36m",  // DEBUG: 青色
    "\033[32m",  // INFO:  绿色
    "\033[33m",  // WARN:  黄色
    "\033[31m"   // ERROR: 红色
};
static const char* color_reset = "\033[0m";

// 初始化日志系统
void log_init(void) {
    gettimeofday(&g_start_time, NULL);
    g_time_initialized = 1;
}

// 设置日志级别
void log_set_level(log_level_t level) {
    g_log_level = level;
}

// 获取时间戳字符串
const char* log_get_timestamp(void) {
    static char buf[64];
    struct timeval now;
    gettimeofday(&now, NULL);

    if (!g_time_initialized) {
        g_start_time = now;
        g_time_initialized = 1;
    }

    // 计算相对时间（毫秒）
    long elapsed_ms = (now.tv_sec - g_start_time.tv_sec) * 1000 +
                      (now.tv_usec - g_start_time.tv_usec) / 1000;

    // 格式: [时:分:秒.毫秒 +相对毫秒ms]
    struct tm* tm_info = localtime(&now.tv_sec);
    snprintf(buf, sizeof(buf), "[%02d:%02d:%02d.%03ld +%4ldms]",
             tm_info->tm_hour, tm_info->tm_min, tm_info->tm_sec,
             (long)(now.tv_usec / 1000), elapsed_ms);
    return buf;
}

// 从路径中提取文件名
static const char* get_filename(const char* path) {
    const char* name = strrchr(path, '/');
    return name ? name + 1 : path;
}

// 带文件名和行号的日志输出函数
void log_output_ex(log_level_t level, const char* file, int line, const char* fmt, ...) {
    if (level < g_log_level) {
        return;
    }

    FILE* out = (level >= LOG_LEVEL_WARN) ? stderr : stdout;
    const char* filename = get_filename(file);

    // 输出格式: [时间戳] [级别] [文件:行号] 消息
    fprintf(out, "%s %s%s%s [%s:%d] ",
            log_get_timestamp(),
            level_colors[level],
            level_names[level],
            color_reset,
            filename,
            line);

    // 输出消息
    va_list args;
    va_start(args, fmt);
    vfprintf(out, fmt, args);
    va_end(args);
}
