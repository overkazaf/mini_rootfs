/*
 * server.c - Socket 服务器示例
 *
 * 教学目的：
 * 1. 展示 TCP 服务器的创建流程: socket() -> bind() -> listen() -> accept()
 * 2. 展示如何处理客户端连接和消息
 * 3. 展示基于协议的消息解析和响应
 *
 * 用法: ./server [-p port]
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <signal.h>

/* Socket 相关头文件 */
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "protocol.h"

/* 全局变量: 服务器 socket (用于信号处理) */
static int g_server_fd = -1;
static volatile int g_running = 1;

/* ============================================
 * 辅助函数
 * ============================================ */

/*
 * 打印带时间戳的日志
 */
void log_msg(const char *fmt, ...) {
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char time_buf[20];
    strftime(time_buf, sizeof(time_buf), "%H:%M:%S", tm_info);

    printf("[%s] ", time_buf);

    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);

    printf("\n");
    fflush(stdout);
}

/*
 * 信号处理函数 - 优雅退出
 */
void signal_handler(int sig) {
    log_msg("Received signal %d, shutting down...", sig);
    g_running = 0;
    if (g_server_fd >= 0) {
        close(g_server_fd);
    }
}

/* ============================================
 * 消息收发函数
 * ============================================ */

/*
 * 接收完整消息
 * 返回: 0=成功, -1=错误, 1=连接关闭
 */
int recv_message(int client_fd, Message *msg) {
    /* 步骤 1: 先接收消息头 (3字节) */
    ssize_t n = recv(client_fd, &msg->header, HEADER_SIZE, MSG_WAITALL);
    if (n == 0) {
        return 1;  /* 连接关闭 */
    }
    if (n != HEADER_SIZE) {
        return -1; /* 错误 */
    }

    /* 步骤 2: 转换字节序并验证长度 */
    uint16_t payload_len = ntohs(msg->header.length);
    if (payload_len > MAX_PAYLOAD_SIZE) {
        return -1;
    }

    /* 步骤 3: 接收负载数据 */
    if (payload_len > 0) {
        n = recv(client_fd, msg->payload, payload_len, MSG_WAITALL);
        if (n != payload_len) {
            return -1;
        }
    }

    return 0;
}

/*
 * 发送完整消息
 */
int send_message(int client_fd, uint8_t cmd, const void *payload, uint16_t len) {
    Message msg;
    msg.header.cmd = cmd;
    msg.header.length = htons(len);

    if (len > 0 && payload != NULL) {
        memcpy(msg.payload, payload, len);
    }

    ssize_t total = HEADER_SIZE + len;
    ssize_t sent = send(client_fd, &msg, total, 0);

    return (sent == total) ? 0 : -1;
}

/* ============================================
 * 命令处理函数
 * ============================================ */

/*
 * 处理 ECHO 命令 - 原样返回数据
 */
void handle_echo(int client_fd, const char *data, uint16_t len) {
    log_msg("  -> ECHO: \"%.*s\"", len, data);
    send_message(client_fd, RESP_OK, data, len);
}

/*
 * 处理 TIME 命令 - 返回服务器时间
 */
void handle_time(int client_fd) {
    time_t now = time(NULL);
    char *time_str = ctime(&now);
    time_str[strlen(time_str) - 1] = '\0';  /* 移除换行符 */

    log_msg("  -> TIME: %s", time_str);
    send_message(client_fd, RESP_OK, time_str, strlen(time_str));
}

/*
 * 处理 INFO 命令 - 返回服务器信息
 */
void handle_info(int client_fd) {
    char info[256];
    snprintf(info, sizeof(info),
             "Server: Mini Socket Server v1.0\n"
             "Protocol: Custom Binary Protocol\n"
             "Max Payload: %d bytes\n"
             "PID: %d",
             MAX_PAYLOAD_SIZE, getpid());

    log_msg("  -> INFO requested");
    send_message(client_fd, RESP_OK, info, strlen(info));
}

/*
 * 处理 PING 命令 - 返回 PONG
 */
void handle_ping(int client_fd) {
    const char *pong = "PONG";
    log_msg("  -> PING -> PONG");
    send_message(client_fd, RESP_OK, pong, strlen(pong));
}

/*
 * 处理计算命令
 */
void handle_calc(int client_fd, uint8_t cmd, const char *data, uint16_t len) {
    if (len < sizeof(CalcPayload)) {
        const char *err = "Invalid calc payload";
        send_message(client_fd, RESP_ERROR, err, strlen(err));
        return;
    }

    CalcPayload *calc = (CalcPayload *)data;
    int32_t a = ntohl(calc->a);
    int32_t b = ntohl(calc->b);
    int32_t result = 0;
    const char *op = "";

    switch (cmd) {
        case CMD_CALC_ADD:
            result = a + b;
            op = "+";
            break;
        case CMD_CALC_SUB:
            result = a - b;
            op = "-";
            break;
        case CMD_CALC_MUL:
            result = a * b;
            op = "*";
            break;
        case CMD_CALC_DIV:
            if (b == 0) {
                const char *err = "Division by zero";
                log_msg("  -> CALC: %d / 0 = ERROR", a);
                send_message(client_fd, RESP_ERROR, err, strlen(err));
                return;
            }
            result = a / b;
            op = "/";
            break;
    }

    log_msg("  -> CALC: %d %s %d = %d", a, op, b, result);

    CalcResult resp;
    resp.result = htonl(result);
    send_message(client_fd, RESP_OK, &resp, sizeof(resp));
}

/* ============================================
 * 客户端处理
 * ============================================ */

/*
 * 处理单个客户端连接
 */
void handle_client(int client_fd, struct sockaddr_in *client_addr) {
    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &client_addr->sin_addr, client_ip, sizeof(client_ip));
    int client_port = ntohs(client_addr->sin_port);

    log_msg("Client connected: %s:%d (fd=%d)", client_ip, client_port, client_fd);

    Message msg;
    int running = 1;

    while (running && g_running) {
        /* 接收消息 */
        int ret = recv_message(client_fd, &msg);
        if (ret == 1) {
            log_msg("Client disconnected: %s:%d", client_ip, client_port);
            break;
        }
        if (ret < 0) {
            log_msg("Error receiving from %s:%d", client_ip, client_port);
            break;
        }

        uint16_t payload_len = ntohs(msg.header.length);
        log_msg("Received [%s] from %s:%d, len=%d",
                cmd_to_string(msg.header.cmd), client_ip, client_port, payload_len);

        /* 根据命令类型分发处理 */
        switch (msg.header.cmd) {
            case CMD_ECHO:
                handle_echo(client_fd, msg.payload, payload_len);
                break;

            case CMD_TIME:
                handle_time(client_fd);
                break;

            case CMD_INFO:
                handle_info(client_fd);
                break;

            case CMD_PING:
                handle_ping(client_fd);
                break;

            case CMD_CALC_ADD:
            case CMD_CALC_SUB:
            case CMD_CALC_MUL:
            case CMD_CALC_DIV:
                handle_calc(client_fd, msg.header.cmd, msg.payload, payload_len);
                break;

            case CMD_QUIT:
                log_msg("  -> Client requested disconnect");
                running = 0;
                break;

            default:
                log_msg("  -> Unknown command: 0x%02X", msg.header.cmd);
                const char *err = "Unknown command";
                send_message(client_fd, RESP_ERROR, err, strlen(err));
                break;
        }
    }

    close(client_fd);
    log_msg("Connection closed: %s:%d", client_ip, client_port);
}

/* ============================================
 * 主函数
 * ============================================ */

void print_usage(const char *prog) {
    printf("Usage: %s [-p port]\n", prog);
    printf("Options:\n");
    printf("  -p port   Server port (default: %d)\n", DEFAULT_PORT);
    printf("  -h        Show this help\n");
}

int main(int argc, char *argv[]) {
    int port = DEFAULT_PORT;
    int opt;

    /* 解析命令行参数 */
    while ((opt = getopt(argc, argv, "p:h")) != -1) {
        switch (opt) {
            case 'p':
                port = atoi(optarg);
                break;
            case 'h':
                print_usage(argv[0]);
                return 0;
            default:
                print_usage(argv[0]);
                return 1;
        }
    }

    /* 设置信号处理 */
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    printf("==========================================\n");
    printf("     Mini Socket Server - 教学示例\n");
    printf("==========================================\n\n");

    /*
     * ========================================
     * 步骤 1: 创建 socket
     * ========================================
     * socket(domain, type, protocol)
     * - AF_INET: IPv4 协议族
     * - SOCK_STREAM: TCP (面向连接的流式套接字)
     * - 0: 自动选择协议 (TCP)
     */
    log_msg("Step 1: Creating socket...");
    g_server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (g_server_fd < 0) {
        perror("socket() failed");
        return 1;
    }
    log_msg("  -> Socket created (fd=%d)", g_server_fd);

    /* 设置地址重用 (避免 "Address already in use" 错误) */
    int reuse = 1;
    setsockopt(g_server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    /*
     * ========================================
     * 步骤 2: 绑定地址
     * ========================================
     * bind(sockfd, addr, addrlen)
     * - 将 socket 绑定到指定的 IP 地址和端口
     */
    log_msg("Step 2: Binding to port %d...", port);

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;         /* IPv4 */
    server_addr.sin_addr.s_addr = INADDR_ANY; /* 监听所有网卡 */
    server_addr.sin_port = htons(port);       /* 端口 (网络字节序) */

    if (bind(g_server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind() failed");
        close(g_server_fd);
        return 1;
    }
    log_msg("  -> Bound to 0.0.0.0:%d", port);

    /*
     * ========================================
     * 步骤 3: 开始监听
     * ========================================
     * listen(sockfd, backlog)
     * - backlog: 等待连接队列的最大长度
     */
    log_msg("Step 3: Starting to listen...");
    if (listen(g_server_fd, 5) < 0) {
        perror("listen() failed");
        close(g_server_fd);
        return 1;
    }
    log_msg("  -> Listening for connections...\n");

    printf("Server is running on port %d\n", port);
    printf("Press Ctrl+C to stop\n");
    printf("------------------------------------------\n\n");

    /*
     * ========================================
     * 步骤 4: 接受连接并处理
     * ========================================
     * accept(sockfd, addr, addrlen)
     * - 阻塞等待客户端连接
     * - 返回新的 socket 用于与该客户端通信
     */
    while (g_running) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);

        log_msg("Waiting for new connection...");
        int client_fd = accept(g_server_fd, (struct sockaddr *)&client_addr, &client_len);

        if (client_fd < 0) {
            if (g_running) {
                perror("accept() failed");
            }
            continue;
        }

        /* 处理客户端 (单线程版本，一次只能处理一个客户端) */
        handle_client(client_fd, &client_addr);
    }

    close(g_server_fd);
    log_msg("Server stopped.");

    return 0;
}
