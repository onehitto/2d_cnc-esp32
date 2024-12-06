#include "main.h"

#define TAG "MAIN"

extern "C" void app_main(void) {
  engine::cnc obj;

  obj.cnc_init();

  xTaskCreate(engine::cnc::cnc_task_entry, "cnc_task", 4096, &obj, 5, NULL);

  while (1) {
    ESP_LOGI(TAG, "Hello cnc !!");
    ESP_LOGI("HEAP", "Free heap: %ld bytes", esp_get_free_heap_size());
    vTaskDelay(pdSECOND * 100);
  }
}