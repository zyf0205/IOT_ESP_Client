#include "websocket_client.h"
#include "protocol.h"
#include "esp_websocket_client.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <inttypes.h>
#include "display.h"
#include "sensor.h"

#define LED_GPIO GPIO_NUM_2 /*ledGPIO*/

static const char *TAG = "WS_CLIENT";
static esp_websocket_client_handle_t client = NULL;

// WebSocket事件回调
static void websocket_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
  esp_websocket_event_data_t *data = (esp_websocket_event_data_t *)event_data;

  switch (event_id)
  {
  case WEBSOCKET_EVENT_CONNECTED:
    ESP_LOGI(TAG, "WEBSOCKET_EVENT_CONNECTED");
    sys_data.is_connected = true;
    break;

  case WEBSOCKET_EVENT_DISCONNECTED:
    ESP_LOGW(TAG, "WEBSOCKET_EVENT_DISCONNECTED");
    sys_data.is_connected = false;
    is_register = 0;
    break;

  case WEBSOCKET_EVENT_DATA:
    // 处理接收到的二进制数据
    if (data->op_code == 0x02 && data->data_len >= 13)
    { // Binary Frame, 最少：Header(11) + Data(0) + CRC(2) = 13 Bytes
      uint8_t *recv = (uint8_t *)data->data_ptr;

      // 快速检查帧头（前两字节）- 提前过滤无效数据
      uint16_t frame_head = recv[0] | (recv[1] << 8);
      if (frame_head != FRAME_HEAD)
      {
        ESP_LOGW(TAG, "Invalid frame header: 0x%04X", frame_head);
        break;
      }

      // 解析 Header
      uint8_t frame_ver = recv[2];
      uint8_t cmd = recv[3];
      uint8_t seq = recv[4];
      uint32_t dev_id = recv[5] | (recv[6] << 8) | (recv[7] << 16) | (recv[8] << 24);
      uint16_t payload_len = recv[9] | (recv[10] << 8);

      // 长度一致性检查：Header(11) + Payload + CRC(2)
      size_t expected_len = 11 + payload_len + 2;
      if (data->data_len != expected_len)
      {
        ESP_LOGW(TAG, "Length mismatch: header payload=%u, frame=%zu", payload_len, data->data_len);
        break;
      }

      // 验证版本
      if (frame_ver != FRAME_VER)
      {
        ESP_LOGW(TAG, "Invalid frame version: 0x%02X", frame_ver);
        break;
      }

      // 验证 CRC（范围：除去最后2字节CRC本身）
      uint16_t received_crc = recv[data->data_len - 2] | (recv[data->data_len - 1] << 8);
      uint16_t calculated_crc = calc_crc16(recv, data->data_len - 2);
      if (received_crc != calculated_crc)
      {
        ESP_LOGW(TAG, "CRC mismatch! Received: 0x%04X, Calculated: 0x%04X", received_crc, calculated_crc);
        break;
      }

      ESP_LOGI(TAG, "Received: Cmd=0x%02X, Seq=%d, DevID=0x%08" PRIx32 ", PayloadLen=%d", cmd, seq, dev_id, payload_len);

      if (cmd == CMD_CONTROL && is_register)
      {
        uint8_t status = recv[11];
        gpio_set_level(LED_GPIO, status ? 1 : 0);
        sys_data.led_status = (status != 0);
      }
      else if (cmd == CMD_REGISTER_ANSWER)
      {
        uint8_t register_answer = recv[11];
        if (register_answer)
        {
          is_register = 1; /*注册成功*/
          ESP_LOGI("register", "register successful");
        }
        else
        {
          is_register = 0; /*注册失败*/
          ESP_LOGI("register", "register failed");
        }
      }
      else if (cmd == CMD_HEARTBEAT_RESP)
      {
        ESP_LOGI(TAG, "收到心跳响应");
      }
      else if (cmd == CMD_ACK)
      {
        ESP_LOGI(TAG, "收到上报确认");
      }
    }
    break;

  case WEBSOCKET_EVENT_ERROR:
    ESP_LOGE(TAG, "WEBSOCKET_EVENT_ERROR");
    break;
  }
}

/*发送数据*/
void ws_send_packet(uint8_t cmd, uint8_t *payload, uint16_t payload_len)
{
  /*先判断客户端是否连接到服务器*/
  if (client == NULL || !esp_websocket_client_is_connected(client))
  {
    return;
  }

  // 计算总长度：Header(11) + Payload(N) + CRC(2)
  // Header组成: Head(2)+Ver(1)+Cmd(1)+Seq(1)+DevID(4)+Len(2) = 11 Bytes
  size_t total_len = 11 + payload_len + 2;

  /*开辟临时缓冲区*/
  uint8_t *buffer = malloc(total_len);
  if (!buffer)
  {
    ESP_LOGE(TAG, "Memory allocation failed");
    return;
  }

  static uint8_t seq = 0; /*帧序列号*/
  size_t idx = 0;         /*缓冲区索引*/

  /*header*/
  // 0xAA55 Little Endian -> 0x55, 0xAA
  write_u16_le(&buffer[idx], FRAME_HEAD);
  idx += 2;
  buffer[idx++] = FRAME_VER;
  buffer[idx++] = cmd;
  buffer[idx++] = seq++;
  write_u32_le(&buffer[idx], DEVICE_ID);
  idx += 4;
  write_u16_le(&buffer[idx], payload_len);
  idx += 2;

  /*Payload*/
  if (payload_len > 0 && payload != NULL)
  {
    memcpy(&buffer[idx], payload, payload_len); /*将payload从参数复制到缓冲区*/
    idx += payload_len;
  }

  /*计算 CRC (计算范围：除去最后2字节CRC本身)*/
  uint16_t crc = calc_crc16(buffer, idx);
  write_u16_le(&buffer[idx], crc);
  idx += 2;

  /*发送，通过二进制流的方式*/
  esp_websocket_client_send_bin(client, (const char *)buffer, total_len, portMAX_DELAY);
  ESP_LOGI(TAG, "Sent Packet: Cmd=0x%02X, Len=%d", cmd, total_len);

  free(buffer);
}

bool ws_is_connected(void)
{
  return (client != NULL && esp_websocket_client_is_connected(client));
}

esp_err_t ws_client_init(void)
{
  // 初始化LED GPIO
  gpio_reset_pin(LED_GPIO);
  gpio_set_direction(LED_GPIO, GPIO_MODE_OUTPUT);

  esp_websocket_client_config_t websocket_cfg = {
      .uri = SERVER_URI,
      .reconnect_timeout_ms = 2000,
  };

  /*初始化websocket客户端*/
  client = esp_websocket_client_init(&websocket_cfg);
  if (client == NULL)
  {
    ESP_LOGE(TAG, "Failed to init websocket client");
    return ESP_FAIL;
  }

  /*为客户端注册事件处理函数，将client句柄传给cb*/
  esp_websocket_register_events(client, WEBSOCKET_EVENT_ANY, websocket_event_handler, (void *)client);

  /*启动客户端*/
  esp_websocket_client_start(client);

  ESP_LOGI(TAG, "WebSocket client initialized");
  return ESP_OK;
}