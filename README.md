# harmonyos-oled-player
使用鸿蒙OS在0.96寸OLED屏上播放视频 | Play video on 0.96' OLED display with Harmony OS



## 简介

* 基于HiSpark WiFi IoT套件（Hi3861芯片），板载0.96寸 128x64 分辨率的 OLED屏
* 板端程序使用了[鸿蒙OS SSD1306 OLED屏驱动库](https://github.com/xusiwei/harmonyos-ssd1306)，用于实现每帧画面的绘制；
* PC端使用了`opencv-python`，用于实现视频解码、画面缩放、二值化和帧数据打包；



## 如何编译

1. 在openharmony顶层目录，下载代码：`git clone --recursive https://gitee.com/hihopeorg/harmonyos_oled_player.git`
2. 修改openharmony源码的`build\lite\product\wifiiot.json`文件：
   * 将其中的`//applications/sample/wifi-iot/app`替换为`//harmonyos-oled-player:app`

3. 在openharmony源码顶层目录下执行：`python build.py wifiiot`



## 如何运行

网络环境：一个无线热点，一台PC，PC连接在该热点上；

### 准备视频资源

1. 选择准备播放的视频，使用工具将帧率转为10fps；
   * 目前ssd1306库实测的最大帧率为10fps；
   * ffmpg转换命令：`ffmpeg -i input.mp4 -r 10 output.mp4`
2. 运行命令：`./video2bin.py output.mp4 out.bin`，将视频转为bin文件；



### 运行程序

1. PC上运行命令：`./bin2stream.py out.bin`，将会启动一个TCP服务器，默认监听`5678`端口
2. 根据热点信息（SSID,PSK）和PC的IP地址，修改`play/net_params.h`文件中的相关参数：
3. 重新编译：`python build.py wifiiot`
4. 将重新编译好的固件烧录到WiFi IoT开发板上，并复位设备；
   * 板子启动后，首先会连上你的热点，然后会连接PC上的TCP服务；
   * 然后就可以看到视频的在OLED屏播放了；



## 原理介绍

整体为C/S架构：

* 使用TCP传输帧数据，实现了简单的二进制协议：
  * 请求格式：帧ID（4B）；
  * 响应格式：状态码（4B） + 帧数据长度（4B）+ 帧数据（可选）；
* PC上运行服务端，默认监听`5678`端口，使用Python开发；
  * 启动时加载整个bin文件，并将其按照帧数据大小分割，放入一个list中；
  * 客户端请求特定帧时，直接根据下标索引取出对应帧，并发送给客户端；
  * 这样的设计（视频预先转换好），可以保证服务端的响应尽可能快；
* 板上运行客户端；
  * 主动发送帧请求，并接收服务端的回应；
  * 收到帧数据后通过I2C向OLED屏发送帧数据；

