#include "report.h"
#include "websocket_client.h"
#include "protocol.h"
#include "net_time.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_random.h"
#include "esp_log.h"
#include <string.h>
#include "dht.h"
#include "display.h"
#include "esp_adc/adc_oneshot.h"

#define DHT_SENSOR_GPIO GPIO_NUM_4
#define LIGHT_SENSOR_ADC_CHANNEL ADC_CHANNEL_6 /*GPIO 34*/

/*是否注册成功*/
bool is_register = false;

/*上报任务*/
static void report_task(void *pvParameters)
{
  // ADC 初始化
  adc_oneshot_unit_handle_t adc1_handle;
  adc_oneshot_unit_init_cfg_t init_config1 = {
      .unit_id = ADC_UNIT_1, // Wi-Fi 开启时必须用 ADC1
  };
  ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config1, &adc1_handle));

  adc_oneshot_chan_cfg_t config = {
      .bitwidth = ADC_BITWIDTH_DEFAULT, // 默认 12位 (0-4095)
      .atten = ADC_ATTEN_DB_12,         // 12dB 衰减，支持 0-3.3V 电压
  };
  ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, LIGHT_SENSOR_ADC_CHANNEL, &config));

  /*传感器值*/
  float temperature = 0;
  float humidity = 0;
  int light_raw = 0;
  /*计数器*/
  volatile static uint32_t tick = 0;
  while (1)
  {
    vTaskDelay(pdMS_TO_TICKS(1000));
    tick++;

    /*注册包*/
    if (tick % 3 == 0)
    {
      if (ws_is_connected() && !is_register)
      {
        uint8_t payload[8];
        uint64_t now_ms = net_get_time_ms();
        write_u32_le(&payload[0], (uint32_t)(now_ms & 0xFFFFFFFF));
        write_u32_le(&payload[4], (uint32_t)(now_ms >> 32));
        ws_send_packet(CMD_REGISTER, payload, sizeof(payload));
        ESP_LOGI("register", "register send");
      }
    }

    /*心跳上报*/
    if (tick % 10 == 0)
    {
      if (ws_is_connected() && is_register)
      {
        uint8_t payload[8];
        uint64_t now_ms = net_get_time_ms();
        write_u32_le(&payload[0], (uint32_t)(now_ms & 0xFFFFFFFF));
        write_u32_le(&payload[4], (uint32_t)(now_ms >> 32));
        ws_send_packet(CMD_HEARTBEAT, payload, sizeof(payload));
        ESP_LOGI("heartbeat", "heartbeat send");
      }
    }

    /*温湿度，光照上报*/
    if (tick % 3 == 0)
    {
      /*读取温湿度*/
      dht_read_float_data(DHT_TYPE_DHT11, DHT_SENSOR_GPIO, &humidity, &temperature);
      /*读取光照 (ADC)*/
      adc_oneshot_read(adc1_handle, LIGHT_SENSOR_ADC_CHANNEL, &light_raw);

      /*更新屏幕数据*/
      sys_data.temperature = temperature;
      sys_data.humidity = humidity;
      sys_data.lux = light_raw;

      if (ws_is_connected() && is_register)
      {
        /*故障通知*/
        if ((temperature == 0 && humidity == 0) || light_raw == 0)
        {
          uint8_t payload[9];
          payload[0] = 0x11; /*传感器故障*/
          uint64_t now_ms = net_get_time_ms();
          write_u32_le(&payload[1], (uint32_t)(now_ms & 0xFFFFFFFF));
          write_u32_le(&payload[5], (uint32_t)(now_ms >> 32));
          ws_send_packet(CMD_FAULT, payload, sizeof(payload));
          ESP_LOGI("fault", "sensor fault");
        }

        // 构造 Payload: Temp(4) + Hum(4) + Light(4) + Time(8) = 20 Bytes
        uint8_t payload[20];
        memset(payload, 0, 20);

        // [修改] 使用 memcpy 发送浮点数，保留精度
        memcpy(&payload[0], &temperature, sizeof(float));
        memcpy(&payload[4], &humidity, sizeof(float));

        // [新增] 写入光照数据 (int -> 4 bytes)
        write_u32_le(&payload[8], (uint32_t)light_raw);

        uint64_t now_ms = net_get_time_ms();
        if (now_ms == 0)
        {
          ESP_LOGW("time", "Time not synced yet!");
        }

        // [修改] 时间戳偏移量向后移动 4 字节 (从 index 12 开始)
        write_u32_le(&payload[12], (uint32_t)(now_ms & 0xFFFFFFFF));
        write_u32_le(&payload[16], (uint32_t)(now_ms >> 32));

        // 发送长度改为 20
        ws_send_packet(CMD_REPORT, payload, 20);

        /*将传感器数值清理，用于判断后续传感器问题*/
        temperature = 0;
        humidity = 0;
      }
    }
  }
}

esp_err_t report_task_start(void)
{
  BaseType_t ret = xTaskCreate(report_task, "sensor_task", 4096, NULL, 5, NULL);
  if (ret != pdPASS)
  {
    ESP_LOGE("report_task", "Failed to create report task");
    return ESP_FAIL;
  }

  ESP_LOGI("report_task", "Sensor task started");
  return ESP_OK;
}