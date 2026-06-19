#!/usr/bin/env bash
# ============================================
# 小智 AI 聊天机器人 - 编译启动脚本
# 板子: Waveshare ESP32-S3-RLCD-4.2
# ============================================
set -e

# 路径配置
export IDF_PATH=/d/esp/v5.5.4/esp-idf
export IDF_TOOLS_PATH=/c/Users/admin/.espressif
export IDF_PYTHON_ENV_PATH=/c/Users/admin/.espressif/python_env/idf5.5_py3.14_env

# 添加工具到 PATH
ESP_TOOLS=(xtensa-esp-elf/esp-14.2.0_20260121/xtensa-esp-elf/bin
           cmake/3.30.2/bin
           ninja/1.12.1
           ccache/4.12.1
           idf-exe/1.0.3
           openocd-esp32/v0.12.0-esp32-20251215
           dfu-util/0.11)

for tool in "${ESP_TOOLS[@]}"; do
    export PATH="$IDF_TOOLS_PATH/tools/$tool:$PATH"
done
export PATH="$IDF_PYTHON_ENV_PATH/Scripts:$PATH"

VENV_PYTHON=$IDF_PYTHON_ENV_PATH/Scripts/python.exe

case "${1:-build}" in
    menuconfig)
        echo ">>> 运行 menuconfig..."
        exec $VENV_PYTHON $IDF_PATH/tools/idf.py menuconfig
        ;;
    build|"")
        echo ">>> 编译 waveshare-s3-rlcd-4.2 固件..."
        exec $VENV_PYTHON $IDF_PATH/tools/idf.py build
        ;;
    flash)
        echo ">>> 编译并烧录..."
        exec $VENV_PYTHON $IDF_PATH/tools/idf.py -p ${2:-COM3} flash
        ;;
    monitor)
        echo ">>> 串口监视器..."
        exec $VENV_PYTHON $IDF_PATH/tools/idf.py -p ${2:-COM3} monitor
        ;;
    release)
        echo ">>> 生成 Release 固件..."
        exec $VENV_PYTHON scripts/release.py waveshare-s3-rlcd-4.2 --name xiaozhi
        ;;
    clean)
        echo ">>> 清理构建目录..."
        exec $VENV_PYTHON $IDF_PATH/tools/idf.py fullclean
        ;;
    *)
        echo "用法: ./build.sh [命令]"
        echo "命令:"
        echo "  build      编译固件 (默认)"
        echo "  menuconfig  配置菜单"
        echo "  flash      编译并烧录"
        echo "  monitor    串口监视器"
        echo "  release    生成 Release 固件"
        echo "  clean      清理"
        exit 1
        ;;
esac
