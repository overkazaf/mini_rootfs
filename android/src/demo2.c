/*
 * demo2.c - Second shared library for multi-so loading demo
 */

#include <stdio.h>
#include <string.h>

/* 导出函数：打印消息 */
void demo2_print(const char *msg) {
    printf("[demo2.so] Message: %s\n", msg);
}

/* 导出函数：字符串长度 */
int demo2_strlen(const char *str) {
    int len = strlen(str);
    printf("[demo2.so] String length of \"%s\" is %d\n", str, len);
    return len;
}

/* 导出函数：乘法计算 */
int demo2_multiply(int a, int b) {
    printf("[demo2.so] Calculating %d * %d\n", a, b);
    return a * b;
}

__attribute__((constructor))
void demo2_init(void) {
    printf("[demo2.so] Library loaded! (constructor called)\n");
}

__attribute__((destructor))
void demo2_fini(void) {
    printf("[demo2.so] Library unloading! (destructor called)\n");
}
