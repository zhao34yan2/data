# 编译环境说明

## 硬件
- **设备**: Waveshare ESP32-S3-RLCD-4.2
- **芯片**: ESP32-S3 (QFN56, 16MB Flash, 8MB OCT PSRAM)

## 编译环境 (Windows)

### 1. 安装 ESP-IDF v5.5.2

下载 [ESP-IDF v5.5.2 离线安装器](https://dl.espressif.com/dl/esp-idf/) 安装到 `E:\ESP\v5.5.2`

或者手动安装：
```bash
git clone -b v5.5.2 https://github.com/espressif/esp-idf.git E:\ESP\v5.5.2\esp-idf
cd E:\ESP\v5.5.2\esp-idf
install.bat esp32s3
```

### 2. 编译

```bash
# 打开 ESP-IDF CMD
cd D:\work\xiaozhi-esp32-main-1

# 首次配置
idf.py set-target esp32s3

# 修改板型 (重要!)
# 编辑 sdkconfig, 把 CONFIG_BOARD_TYPE_BREAD_COMPACT_WIFI=y
# 改为 CONFIG_BOARD_TYPE_WAVESHARE_S3_RLCD_4_2=y
# 添加 CONFIG_USE_DEVICE_AEC=y

# 创建 secret_config.h
cp main/boards/waveshare-s3-rlcd-4.2/secret_config.h.example main/boards/waveshare-s3-rlcd-4.2/secret_config.h
# 编辑填入城市坐标等信息

# 编译 & 烧录
idf.py build
idf.py -p COM4 flash
```

### 3. Python 环境

- Python 3.13
- 虚拟环境路径: `C:\Users\用户名\.espressif\python_env\idf5.5_py3.13_env`

### 4. 工具链

| 工具 | 路径 |
|------|------|
| Xtensa GCC | `~/.espressif/tools/xtensa-esp-elf/esp-14.2.0_20251107/` |
| CMake | `~/.espressif/tools/cmake/3.30.2/` |
| Ninja | `~/.espressif/tools/ninja/` |
| esptool | ESP-IDF 内置 |

## 关键配置

| 配置项 | 值 |
|--------|-----|
| Target | esp32s3 |
| Board | WAVESHARE_S3_RLCD_4_2 |
| Flash | 16MB QIO |
| PSRAM | 8MB OCT 80MHz |
| 唤醒词 | 你好小智 (WN9) |
| 语言 | 中文 (zh-CN) |
| 配网 | 热点配网 |
| AEC | 设备端 AEC |
