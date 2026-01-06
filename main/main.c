#include <stdio.h>
#include "esp_log.h"
#include "nvs_flash.h"
#include "wifi_mgr.h"
#include "websocket_client.h"
#include "report.h"
#include "display.h"

void app_main(void)
{
  // 1. 初始化 NVS
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
  {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);

  // 启动上报任务
  report_task_start();

  display_task_start();

  // 初始化并连接 WiFi
  ESP_ERROR_CHECK(wifi_mgr_init());

  // 初始化 WebSocket 客户端
  ESP_ERROR_CHECK(ws_client_init());
}