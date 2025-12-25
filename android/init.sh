#!/system/bin/sh
#
# Android rootfs init script
# 启动时加载动态库并执行测试程序

echo "========================================"
echo "Android Mini Rootfs - Initializing..."
echo "========================================"

# 设置 library 路径
export LD_LIBRARY_PATH=/lib:/system/lib:$LD_LIBRARY_PATH

# 切换到根目录
cd /

# 运行 loader 程序
if [ -f /bin/loader ]; then
    echo "Running loader..."
    /bin/loader
else
    echo "Error: /bin/loader not found"
fi

echo "========================================"
echo "Init complete."
echo "========================================"
