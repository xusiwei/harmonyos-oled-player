/*
 * Copyright (c) 2020, HiHope Community.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <unistd.h>

#include "ohos_init.h"
#include "cmsis_os2.h"
#include "wifiiot_gpio.h"
#include "wifiiot_gpio_ex.h"
#include "wifiiot_pwm.h"
#include "wifiiot_adc.h"
#include "wifiiot_i2c.h"
#include "wifiiot_errno.h"
#include "wifiiot_watchdog.h"
#include "wifi_device.h"

#include "lwip/netifapi.h"
#include "lwip/api_shell.h"
#include "lwip/sockets.h"

#include "ssd1306.h"
#include "net_params.h"

#define OLED_I2C_BAUDRATE 400*1000

#define STATUS_OK 0

static void PrintLinkedInfo(WifiLinkedInfo* info)
{
    if (!info) return;

    static char macAddress[32] = {0};
    unsigned char* mac = info->bssid;
    snprintf(macAddress, sizeof(macAddress), "%02X:%02X:%02X:%02X:%02X:%02X",
        mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    printf("bssid: %s, rssi: %d, connState: %d, reason: %d, ssid: %s\r\n",
        macAddress, info->rssi, info->connState, info->disconnectedReason, info->ssid);
}

static int g_connected = 0;
static void OnWifiConnectionChanged(int state, WifiLinkedInfo* info)
{
    if (!info) return;

    printf("%s %d, state = %d, info = \r\n", __FUNCTION__, __LINE__, state);
    PrintLinkedInfo(info);

    if (state == WIFI_STATE_AVALIABLE) {
        g_connected = 1;
    } else {
        g_connected = 0;
    }
}

static void OnWifiScanStateChanged(int state, int size)
{
    printf("%s %d, state = %X, size = %d\r\n", __FUNCTION__, __LINE__, state, size);
}

static int ConnectToHotspot(WifiEvent* eventListener, WifiDeviceConfig* apConfig)
{
    WifiErrorCode errCode;
    int netId = -1;

    errCode = RegisterWifiEvent(eventListener);
    printf("RegisterWifiEvent: %d\r\n", errCode);

    errCode = EnableWifi();
    printf("EnableWifi: %d\r\n", errCode);

    errCode = AddDeviceConfig(apConfig, &netId);
    printf("AddDeviceConfig: %d\r\n", errCode);

    g_connected = 0;
    errCode = ConnectTo(netId);
    printf("ConnectTo(%d): %d\r\n", netId, errCode);

    while (!g_connected) { // wait until connect to AP
        osDelay(10);
    }
    printf("g_connected: %d\r\n", g_connected);

    struct netif* iface = netifapi_netif_find("wlan0");
    if (iface) {
        err_t ret = netifapi_dhcp_start(iface);
        printf("netifapi_dhcp_start: %d\r\n", ret);

        osDelay(100); // wait DHCP server give me IP
        ret = netifapi_netif_common(iface, dhcp_clients_info_show, NULL);
        printf("netifapi_netif_common: %d\r\n", ret);
    }
    return netId;
}

static void DisconnectWithHotspot(WifiEvent* eventListener, int netId)
{
    WifiErrorCode errCode;
    errCode = Disconnect(); // disconnect with your AP
    printf("Disconnect: %d\r\n", errCode);

    errCode = UnRegisterWifiEvent(eventListener);
    printf("UnRegisterWifiEvent: %d\r\n", errCode);

    RemoveDevice(netId); // remove AP config
    printf("RemoveDevice: %d\r\n", errCode);

    errCode = DisableWifi();
    printf("DisableWifi: %d\r\n", errCode);
}

static void PrepareHotspotConfig(WifiDeviceConfig* apConfig)
{
    if (!apConfig) return;
    // setup your AP params
    strcpy(apConfig->ssid, PARAM_HOTSPOT_SSID);
    strcpy(apConfig->preSharedKey, PARAM_HOTSPOT_PSK);
    apConfig->securityType = WIFI_SEC_TYPE_PSK;
}

static int g_serverPort = PARAM_SERVER_PORT;
static const char* g_serverIp = PARAM_SERVER_ADDR;

static uint8_t g_streamBuffer[SSD1306_BUFFER_SIZE];

static uint32_t PlayStream(void)
{
    int sockfd = -1;
    struct sockaddr_in serverAddr = {0};
    uint32_t frameId = 0;
    
    sockfd = lwip_socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        printf("lwip_socket failed!\r\n");
        return frameId;
    }

    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = lwip_htons(g_serverPort);
    if (lwip_inet_pton(AF_INET, g_serverIp, &serverAddr.sin_addr) <= 0) {
        printf("lwip_inet_pton failed!\r\n");
        goto do_close;
    }

    if (lwip_connect(sockfd, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0) {
        printf("lwip_connect failed!\r\n");
        goto do_close;
    }
    printf("connect to server %s success!\r\n", g_serverIp);

    frameId = 1;
    while (1) {
        ssize_t retval = 0;
        uint32_t request = lwip_htonl(frameId); // to big endian
        uint32_t status = 0;
        ssize_t bodyLen = 0;
        ssize_t bodyReceived = 0;

        printf("request frameId %d to server\r\n", frameId);
        retval = lwip_send(sockfd, &request, sizeof(request), 0);
        if (retval < 0) {
            printf("lwip_send for frame %d failed!\r\n", frameId);
            break;
        }

        // printf("recving status from server...\r\n");
        retval = lwip_recv(sockfd, &status, sizeof(status), 0);
        if (retval != sizeof(status)) {
            printf("lwip_recv status for frame %d failed or done, %d!\r\n", frameId, retval);
            break;
        }
        status = lwip_ntohl(status);
        if (status != STATUS_OK) {
            break;
        }

        // printf("recving bodyLen from server...\r\n");
        retval = lwip_recv(sockfd, &bodyLen, sizeof(bodyLen), 0);
        if (retval != sizeof(bodyLen)) {
            printf("lwip_recv bodyLen for frame %d failed or done, %d!\r\n", frameId, retval);
            break;
        }
        bodyLen = lwip_ntohl(bodyLen);
        printf("status: %d, bodyLen: %d\r\n", status, bodyLen);

        while (bodyReceived < bodyLen) {
            retval = lwip_recv(sockfd, &g_streamBuffer[bodyReceived], bodyLen, 0);
            if (retval != bodyLen) {
                printf("lwip_recv for frame %d failed or done, %d!\r\n", frameId, retval);
                break;
            }
            bodyReceived += retval;
            printf("recved body %d/%d...\r\n", bodyReceived, bodyLen);
        }

        ssd1306_Fill(Black);
        ssd1306_DrawBitmap(g_streamBuffer, sizeof(g_streamBuffer));
        ssd1306_UpdateScreen();
        frameId++;
    }
    printf("playing video done, played frames: %d!\r\n", frameId);

do_close:
    lwip_close(sockfd);
    return frameId;
}

static void Ssd1306PlayTask(void* arg)
{
    (void) arg;
    WifiDeviceConfig apConfig = {};
    WifiEvent eventListener = {
        .OnWifiConnectionChanged = OnWifiConnectionChanged,
        .OnWifiScanStateChanged = OnWifiScanStateChanged
    };
    int netId = -1;

    GpioInit();
    IoSetFunc(WIFI_IOT_IO_NAME_GPIO_13, WIFI_IOT_IO_FUNC_GPIO_13_I2C0_SDA);
    IoSetFunc(WIFI_IOT_IO_NAME_GPIO_14, WIFI_IOT_IO_FUNC_GPIO_14_I2C0_SCL);
    I2cInit(WIFI_IOT_I2C_IDX_0, OLED_I2C_BAUDRATE);

    WatchDogDisable();

    PrepareHotspotConfig(&apConfig);

    osDelay(2);
    ssd1306_Init();

    ssd1306_Fill(Black);
    ssd1306_SetCursor(0, 0);
    ssd1306_DrawString("Hello HarmonyOS!", Font_7x10, White);
    ssd1306_UpdateScreen();

    netId = ConnectToHotspot(&eventListener, &apConfig);
    if (netId < 0) {
        printf("connect to hotspot failed!\r\n");
    }

    uint32_t start = osKernelGetTickCount();
    uint32_t frames = PlayStream();
    uint32_t end = osKernelGetTickCount();

    printf("frames: %d, time cost: %.2f\r\n", frames, (end - start) / (float)osKernelGetTickFreq());
    osDelay(3000);

    DisconnectWithHotspot(&eventListener, netId);
}

static void Ssd1306PlayDemo(void)
{
    osThreadAttr_t attr;

    attr.name = "Ssd1306Task";
    attr.attr_bits = 0U;
    attr.cb_mem = NULL;
    attr.cb_size = 0U;
    attr.stack_mem = NULL;
    attr.stack_size = 10240;
    attr.priority = osPriorityNormal;

    if (osThreadNew(Ssd1306PlayTask, NULL, &attr) == NULL) {
        printf("[Ssd1306PlayDemo] Falied to create Ssd1306PlayTask!\n");
    }
}
APP_FEATURE_INIT(Ssd1306PlayDemo);
