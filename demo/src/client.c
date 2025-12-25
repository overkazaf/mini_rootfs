/*
 * client.c - Socket 客户端示例
 *
 * 教学目的：
 * 1. 展示 TCP 客户端的创建流程: socket() -> connect()
 * 2. 展示如何发送请求和接收响应
 * 3. 展示命令行参数解析和交互式操作
 *
 * 用法:
 *   ./client -h <host> -p <port> -c <command> [args]
 *   ./client -i                  # 交互模式
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

/* Socket 相关头文件 */
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include "protocol.h"

/* ============================================
 * 消息收发函数
 * ============================================ */

/*
 * 发送消息到服务器
 */
int send_message(int sock_fd, uint8_t cmd, const void *payload, uint16_t len) {
    Message msg;
    msg.header.cmd = cmd;
    msg.header.length = htons(len);

    if (len > 0 && payload != NULL) {
        memcpy(msg.payload, payload, len);
    }

    ssize_t total = HEADER_SIZE + len;
    ssize_t sent = send(sock_fd, &msg, total, 0);

    return (sent == total) ? 0 : -1;
}

/*
 * 从服务器接收消息
 */
int recv_message(int sock_fd, Message *msg) {
    /* 接收消息头 */
    ssize_t n = recv(sock_fd, &msg->header, HEADER_SIZE, MSG_WAITALL);
    if (n <= 0) {
        return -1;
    }

    /* 接收负载 */
    uint16_t payload_len = ntohs(msg->header.length);
    if (payload_len > 0) {
        n = recv(sock_fd, msg->payload, payload_len, MSG_WAITALL);
        if (n != payload_len) {
            return -1;
        }
        msg->payload[payload_len] = '\0';  /* 确保字符串结尾 */
    }

    return 0;
}

/* ============================================
 * 命令执行函数
 * ============================================ */

/*
 * 执行 ECHO 命令
 */
int do_echo(int sock_fd, const char *text) {
    printf("Sending ECHO: \"%s\"\n", text);

    if (send_message(sock_fd, CMD_ECHO, text, strlen(text)) < 0) {
        perror("Failed to send");
        return -1;
    }

    Message resp;
    if (recv_message(sock_fd, &resp) < 0) {
        perror("Failed to receive");
        return -1;
    }

    uint16_t len = ntohs(resp.header.length);
    printf("Response [%s]: %.*s\n", cmd_to_string(resp.header.cmd), len, resp.payload);
    return 0;
}

/*
 * 执行 TIME 命令
 */
int do_time(int sock_fd) {
    printf("Requesting server time...\n");

    if (send_message(sock_fd, CMD_TIME, NULL, 0) < 0) {
        perror("Failed to send");
        return -1;
    }

    Message resp;
    if (recv_message(sock_fd, &resp) < 0) {
        perror("Failed to receive");
        return -1;
    }

    printf("Server time: %s\n", resp.payload);
    return 0;
}

/*
 * 执行 INFO 命令
 */
int do_info(int sock_fd) {
    printf("Requesting server info...\n");

    if (send_message(sock_fd, CMD_INFO, NULL, 0) < 0) {
        perror("Failed to send");
        return -1;
    }

    Message resp;
    if (recv_message(sock_fd, &resp) < 0) {
        perror("Failed to receive");
        return -1;
    }

    printf("Server info:\n%s\n", resp.payload);
    return 0;
}

/*
 * 执行 PING 命令
 */
int do_ping(int sock_fd) {
    printf("Sending PING...\n");

    if (send_message(sock_fd, CMD_PING, NULL, 0) < 0) {
        perror("Failed to send");
        return -1;
    }

    Message resp;
    if (recv_message(sock_fd, &resp) < 0) {
        perror("Failed to receive");
        return -1;
    }

    printf("Response: %s\n", resp.payload);
    return 0;
}

/*
 * 执行计算命令
 */
int do_calc(int sock_fd, uint8_t cmd, int a, int b) {
    const char *op = "";
    switch (cmd) {
        case CMD_CALC_ADD: op = "+"; break;
        case CMD_CALC_SUB: op = "-"; break;
        case CMD_CALC_MUL: op = "*"; break;
        case CMD_CALC_DIV: op = "/"; break;
    }

    printf("Calculating: %d %s %d\n", a, op, b);

    CalcPayload calc;
    calc.a = htonl(a);
    calc.b = htonl(b);

    if (send_message(sock_fd, cmd, &calc, sizeof(calc)) < 0) {
        perror("Failed to send");
        return -1;
    }

    Message resp;
    if (recv_message(sock_fd, &resp) < 0) {
        perror("Failed to receive");
        return -1;
    }

    if (resp.header.cmd == RESP_OK) {
        CalcResult *result = (CalcResult *)resp.payload;
        printf("Result: %d %s %d = %d\n", a, op, b, ntohl(result->result));
    } else {
        printf("Error: %s\n", resp.payload);
    }

    return 0;
}

/*
 * 执行 QUIT 命令
 */
int do_quit(int sock_fd) {
    printf("Sending QUIT...\n");
    send_message(sock_fd, CMD_QUIT, NULL, 0);
    return 0;
}

/* ============================================
 * 连接管理
 * ============================================ */

/*
 * 连接到服务器
 */
int connect_to_server(const char *host, int port) {
    printf("==========================================\n");
    printf("     Mini Socket Client - 教学示例\n");
    printf("==========================================\n\n");

    /*
     * ========================================
     * 步骤 1: 创建 socket
     * ========================================
     */
    printf("Step 1: Creating socket...\n");
    int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd < 0) {
        perror("socket() failed");
        return -1;
    }
    printf("  -> Socket created (fd=%d)\n", sock_fd);

    /*
     * ========================================
     * 步骤 2: 解析服务器地址
     * ========================================
     */
    printf("Step 2: Resolving host '%s'...\n", host);

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);

    /* 尝试直接解析 IP 地址 */
    if (inet_pton(AF_INET, host, &server_addr.sin_addr) <= 0) {
        /* 如果失败，尝试 DNS 解析 */
        struct hostent *he = gethostbyname(host);
        if (he == NULL) {
            fprintf(stderr, "Cannot resolve host: %s\n", host);
            close(sock_fd);
            return -1;
        }
        memcpy(&server_addr.sin_addr, he->h_addr_list[0], he->h_length);
    }

    char ip_str[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &server_addr.sin_addr, ip_str, sizeof(ip_str));
    printf("  -> Resolved to: %s\n", ip_str);

    /*
     * ========================================
     * 步骤 3: 连接服务器
     * ========================================
     * connect(sockfd, addr, addrlen)
     * - 与服务器建立 TCP 连接 (三次握手)
     */
    printf("Step 3: Connecting to %s:%d...\n", ip_str, port);

    if (connect(sock_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect() failed");
        close(sock_fd);
        return -1;
    }

    printf("  -> Connected successfully!\n\n");
    printf("------------------------------------------\n\n");

    return sock_fd;
}

/* ============================================
 * 交互模式
 * ============================================ */

void print_interactive_help(void) {
    printf("\nAvailable commands:\n");
    printf("  echo <text>       - Echo text back from server\n");
    printf("  time              - Get server time\n");
    printf("  info              - Get server info\n");
    printf("  ping              - Ping server\n");
    printf("  add <a> <b>       - Calculate a + b\n");
    printf("  sub <a> <b>       - Calculate a - b\n");
    printf("  mul <a> <b>       - Calculate a * b\n");
    printf("  div <a> <b>       - Calculate a / b\n");
    printf("  quit              - Disconnect and exit\n");
    printf("  help              - Show this help\n");
    printf("\n");
}

void interactive_mode(int sock_fd) {
    char line[256];
    char cmd[32];
    char arg1[128];
    int a, b;

    print_interactive_help();

    while (1) {
        printf("> ");
        fflush(stdout);

        if (fgets(line, sizeof(line), stdin) == NULL) {
            break;
        }

        /* 去除换行符 */
        line[strcspn(line, "\n")] = '\0';

        if (strlen(line) == 0) {
            continue;
        }

        /* 解析命令 */
        if (sscanf(line, "%31s", cmd) != 1) {
            continue;
        }

        if (strcmp(cmd, "echo") == 0) {
            char *text = line + 5;
            while (*text == ' ') text++;
            if (strlen(text) > 0) {
                do_echo(sock_fd, text);
            } else {
                printf("Usage: echo <text>\n");
            }
        }
        else if (strcmp(cmd, "time") == 0) {
            do_time(sock_fd);
        }
        else if (strcmp(cmd, "info") == 0) {
            do_info(sock_fd);
        }
        else if (strcmp(cmd, "ping") == 0) {
            do_ping(sock_fd);
        }
        else if (strcmp(cmd, "add") == 0) {
            if (sscanf(line, "%*s %d %d", &a, &b) == 2) {
                do_calc(sock_fd, CMD_CALC_ADD, a, b);
            } else {
                printf("Usage: add <a> <b>\n");
            }
        }
        else if (strcmp(cmd, "sub") == 0) {
            if (sscanf(line, "%*s %d %d", &a, &b) == 2) {
                do_calc(sock_fd, CMD_CALC_SUB, a, b);
            } else {
                printf("Usage: sub <a> <b>\n");
            }
        }
        else if (strcmp(cmd, "mul") == 0) {
            if (sscanf(line, "%*s %d %d", &a, &b) == 2) {
                do_calc(sock_fd, CMD_CALC_MUL, a, b);
            } else {
                printf("Usage: mul <a> <b>\n");
            }
        }
        else if (strcmp(cmd, "div") == 0) {
            if (sscanf(line, "%*s %d %d", &a, &b) == 2) {
                do_calc(sock_fd, CMD_CALC_DIV, a, b);
            } else {
                printf("Usage: div <a> <b>\n");
            }
        }
        else if (strcmp(cmd, "quit") == 0 || strcmp(cmd, "exit") == 0) {
            do_quit(sock_fd);
            break;
        }
        else if (strcmp(cmd, "help") == 0) {
            print_interactive_help();
        }
        else {
            printf("Unknown command: %s (type 'help' for commands)\n", cmd);
        }

        printf("\n");
    }
}

/* ============================================
 * 主函数
 * ============================================ */

void print_usage(const char *prog) {
    printf("Usage: %s [options]\n\n", prog);
    printf("Connection options:\n");
    printf("  -h host   Server hostname or IP (default: 127.0.0.1)\n");
    printf("  -p port   Server port (default: %d)\n", DEFAULT_PORT);
    printf("\nMode options:\n");
    printf("  -i        Interactive mode\n");
    printf("\nCommand options (non-interactive):\n");
    printf("  -c cmd    Command to execute:\n");
    printf("            echo <text>  - Echo text\n");
    printf("            time         - Get server time\n");
    printf("            info         - Get server info\n");
    printf("            ping         - Ping server\n");
    printf("            add <a> <b>  - Calculate a + b\n");
    printf("            sub <a> <b>  - Calculate a - b\n");
    printf("            mul <a> <b>  - Calculate a * b\n");
    printf("            div <a> <b>  - Calculate a / b\n");
    printf("\nExamples:\n");
    printf("  %s -i                           # Interactive mode\n", prog);
    printf("  %s -c ping                      # Single ping\n", prog);
    printf("  %s -c \"echo Hello World\"        # Echo message\n", prog);
    printf("  %s -c \"add 10 20\"               # Calculate 10 + 20\n", prog);
    printf("  %s -h 192.168.1.100 -p 9999 -i  # Connect to remote\n", prog);
}

int main(int argc, char *argv[]) {
    const char *host = "127.0.0.1";
    int port = DEFAULT_PORT;
    int interactive = 0;
    const char *command = NULL;
    int opt;

    /* 解析命令行参数 */
    while ((opt = getopt(argc, argv, "h:p:ic:?")) != -1) {
        switch (opt) {
            case 'h':
                host = optarg;
                break;
            case 'p':
                port = atoi(optarg);
                break;
            case 'i':
                interactive = 1;
                break;
            case 'c':
                command = optarg;
                break;
            case '?':
            default:
                print_usage(argv[0]);
                return 1;
        }
    }

    /* 必须指定交互模式或命令 */
    if (!interactive && command == NULL) {
        print_usage(argv[0]);
        return 1;
    }

    /* 连接服务器 */
    int sock_fd = connect_to_server(host, port);
    if (sock_fd < 0) {
        return 1;
    }

    if (interactive) {
        /* 交互模式 */
        interactive_mode(sock_fd);
    } else {
        /* 单命令模式 */
        char cmd[32];
        char arg1[128] = "";
        int a = 0, b = 0;

        sscanf(command, "%31s", cmd);

        if (strcmp(cmd, "echo") == 0) {
            const char *text = command + 5;
            while (*text == ' ') text++;
            do_echo(sock_fd, text);
        }
        else if (strcmp(cmd, "time") == 0) {
            do_time(sock_fd);
        }
        else if (strcmp(cmd, "info") == 0) {
            do_info(sock_fd);
        }
        else if (strcmp(cmd, "ping") == 0) {
            do_ping(sock_fd);
        }
        else if (strcmp(cmd, "add") == 0) {
            sscanf(command, "%*s %d %d", &a, &b);
            do_calc(sock_fd, CMD_CALC_ADD, a, b);
        }
        else if (strcmp(cmd, "sub") == 0) {
            sscanf(command, "%*s %d %d", &a, &b);
            do_calc(sock_fd, CMD_CALC_SUB, a, b);
        }
        else if (strcmp(cmd, "mul") == 0) {
            sscanf(command, "%*s %d %d", &a, &b);
            do_calc(sock_fd, CMD_CALC_MUL, a, b);
        }
        else if (strcmp(cmd, "div") == 0) {
            sscanf(command, "%*s %d %d", &a, &b);
            do_calc(sock_fd, CMD_CALC_DIV, a, b);
        }
        else {
            printf("Unknown command: %s\n", cmd);
        }

        do_quit(sock_fd);
    }

    close(sock_fd);
    printf("Disconnected.\n");

    return 0;
}
