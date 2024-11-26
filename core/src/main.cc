#include "main.h"

#define TAG "MAIN"

extern "C" void app_main(void) {
  // engine::cnc obj;
  // network::webserver ws;

  // ws.webserver_init();

  while (1) {
    ESP_LOGI(TAG, "Hello cnc !!");
    vTaskDelay(pdSECOND * 100);
  }
}