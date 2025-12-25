/*
 * demo.c - A simple shared library for Android rootfs demo
 */

#include <stdio.h>

/* 导出函数：打印欢迎信息 */
void demo_hello(void) {
    printf("[demo.so] Hello from demo shared library!\n");
    printf("[demo.so] This function was loaded dynamically via dlopen.\n");
}

/* 导出函数：加法计算 */
int demo_add(int a, int b) {
    printf("[demo.so] Calculating %d + %d\n", a, b);
    return a + b;
}

/* 导出函数：获取版本信息 */
const char* demo_version(void) {
    return "Demo Library v1.0 for Android rootfs";
}

/* 库加载时自动调用 */
__attribute__((constructor))
void demo_init(void) {
    printf("[demo.so] Library loaded! (constructor called)\n");
}

/* 库卸载时自动调用 */
__attribute__((destructor))
void demo_fini(void) {
    printf("[demo.so] Library unloading! (destructor called)\n");
}
