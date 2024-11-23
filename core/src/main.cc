#include "main.h"

#include "cnc.h"
#include "esp_log.h"
#include "webserver.h"

#define TAG "MAIN"

extern "C" void app_main(void) {
  engine::cnc obj;
  network::webserver ws;

  while (1) {
    ESP_LOGI(TAG, "Hello cnc !!");
    vTaskDelay(pdSECOND * 100);
  }
}