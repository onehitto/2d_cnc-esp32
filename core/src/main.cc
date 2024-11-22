#include "main.h"
#include "esp_err.h"
#include "esp_log.h"

#include "cnc.h"
#include "webserver.h"

#define TAG "MAIN"

extern "C" void app_main(void) {
  engine::cnc obj;
  network::webserver wb;

  while (1) {
    ESP_LOGI(TAG, "Hello cnc !!");
    vTaskDelay(pdSECOND * 100);
  }
}