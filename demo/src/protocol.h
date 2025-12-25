/*
 * protocol.h - Socket 通信协议定义
 *
 * 教学目的：展示如何定义一个简单的应用层协议
 */

#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>

/*
 * ============================================
 * 协议设计说明
 * ============================================
 *
 * 消息格式:
 * +--------+--------+------------------+
 * | CMD(1) | LEN(2) | PAYLOAD(变长)    |
 * +--------+--------+------------------+
 *
 * CMD: 命令类型 (1 字节)
 * LEN: 负载长度 (2 字节, 网络字节序)
 * PAYLOAD: 数据负载 (0-1024 字节)
 */

/* 默认端口 */
#define DEFAULT_PORT 8888

/* 最大负载大小 */
#define MAX_PAYLOAD_SIZE 1024

/* 消息头大小 */
#define HEADER_SIZE 3

/* ============ 命令类型定义 ============ */

typedef enum {
    /* 基础命令 */
    CMD_ECHO      = 0x01,   /* 回显: 服务器原样返回数据 */
    CMD_TIME      = 0x02,   /* 时间: 获取服务器时间 */
    CMD_INFO      = 0x03,   /* 信息: 获取服务器信息 */

    /* 计算命令 */
    CMD_CALC_ADD  = 0x10,   /* 加法: a + b */
    CMD_CALC_SUB  = 0x11,   /* 减法: a - b */
    CMD_CALC_MUL  = 0x12,   /* 乘法: a * b */
    CMD_CALC_DIV  = 0x13,   /* 除法: a / b */

    /* 控制命令 */
    CMD_PING      = 0x20,   /* 心跳检测 */
    CMD_QUIT      = 0xFF,   /* 断开连接 */

    /* 响应状态 */
    RESP_OK       = 0x00,   /* 成功 */
    RESP_ERROR    = 0xFE,   /* 错误 */
} CommandType;

/* ============ 消息结构体 ============ */

/* 消息头 */
typedef struct {
    uint8_t  cmd;           /* 命令类型 */
    uint16_t length;        /* 负载长度 (网络字节序) */
} __attribute__((packed)) MessageHeader;

/* 完整消息 */
typedef struct {
    MessageHeader header;
    char payload[MAX_PAYLOAD_SIZE];
} Message;

/* 计算请求的负载格式 */
typedef struct {
    int32_t a;
    int32_t b;
} __attribute__((packed)) CalcPayload;

/* 计算响应的负载格式 */
typedef struct {
    int32_t result;
} __attribute__((packed)) CalcResult;

/* ============ 辅助函数声明 ============ */

/*
 * 获取命令名称 (用于日志输出)
 */
static inline const char* cmd_to_string(uint8_t cmd) {
    switch (cmd) {
        case CMD_ECHO:     return "ECHO";
        case CMD_TIME:     return "TIME";
        case CMD_INFO:     return "INFO";
        case CMD_CALC_ADD: return "CALC_ADD";
        case CMD_CALC_SUB: return "CALC_SUB";
        case CMD_CALC_MUL: return "CALC_MUL";
        case CMD_CALC_DIV: return "CALC_DIV";
        case CMD_PING:     return "PING";
        case CMD_QUIT:     return "QUIT";
        case RESP_OK:      return "OK";
        case RESP_ERROR:   return "ERROR";
        default:           return "UNKNOWN";
    }
}

#endif /* PROTOCOL_H */
