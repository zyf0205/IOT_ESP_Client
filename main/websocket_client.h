#ifndef WEBSOCKET_CLIENT_H
#define WEBSOCKET_CLIENT_H

#include <stdint.h>
#include "esp_err.h"
#include <stdbool.h>

// WebSocket 服务器配置
#define SERVER_URI "ws://192.168.80.181:8765"
#define DEVICE_ID 0x01020304

// 初始化 WebSocket 客户端
esp_err_t ws_client_init(void);

// 发送协议数据包
void ws_send_packet(uint8_t cmd, uint8_t *payload, uint16_t payload_len);

// 检查连接状态
bool ws_is_connected(void);

#endif // WEBSOCKET_CLIENT_H