#include "main.h"

#include "cnc.h"
#include "esp_log.h"
#include "webserver.h"

#define TAG "MAIN"

extern "C" void app_main(void) {
  engine::cnc obj;
  network::webserver ws;
  strncpy(ws.ssid_buffer, "ssid", sizeof(ws.ssid_buffer) - 1);
  ws.ssid_buffer[sizeof(ws.ssid_buffer) - 1] = '\0';

  strncpy(ws.password_buffer, "password", sizeof(ws.password_buffer) - 1);
  ws.password_buffer[sizeof(ws.password_buffer) - 1] = '\0';

  ws.webserver_init();

  while (1) {
    ESP_LOGI(TAG, "Hello cnc !!");
    vTaskDelay(pdSECOND * 100);
  }
}