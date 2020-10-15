# harmonyos-oled-player
使用鸿蒙OS在0.96寸OLED屏上播放视频 | Play video on 0.96' OLED display with Harmony OS

## 概述

* 基于HiSpark WiFi IoT套件（Hi3861芯片），板载0.96寸 128x64 分辨率的 OLED屏
* 整体方案为C/S架构，使用TCP传输帧数据，PC上运行服务端默认监听`5678`端口，实现了简单的二进制协议：
  * 请求格式：4字节帧ID；
  * 响应格式：4字节状态码 + 4字节帧数据长度+ 边长帧数据；
* 板端程序使用了[鸿蒙OS SSD1306 OLED屏驱动库](https://github.com/xusiwei/harmonyos-ssd1306)，用于实现每帧画面的绘制；
* PC端使用了`opencv-python`，用于实现视频解码、画面缩放、二值化和帧数据打包；


