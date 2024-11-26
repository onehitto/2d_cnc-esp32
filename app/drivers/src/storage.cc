#include "storage.h"

namespace storage {

#define TAG "STORAGE"

esp_vfs_spiffs_conf_t storage::conf = {};
size_t storage::total = 0, storage::used = 0;

const char* storage::path_file = "/storage/execnc.txt";
FILE* storage::file = NULL;

storage::storage() {}

void storage::init_spiffs() {
  esp_vfs_spiffs_conf_t conf = {.base_path = "/storage",
                                .partition_label = NULL,
                                .max_files = 5,
                                .format_if_mount_failed = true};

  esp_err_t ret = esp_vfs_spiffs_register(&conf);

  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
    return;
  }
  ESP_LOGI(TAG, "SPIFFS initialized");
}

size_t storage::Remaing_space() {
  esp_spiffs_info(NULL, &total, &used);
  ESP_LOGI(TAG, "Total: %d, Used: %d", total, used);
  return total - used;
}

esp_err_t storage::write_file(const char* content) {
  if (file == NULL) {
    ESP_LOGE(TAG, "Failed to open file for writing");
    return ESP_FAIL;
  }
  if (fprintf(file, "%s", content) < 0) {
    ESP_LOGE(TAG, "Failed to write to file");
    return ESP_FAIL;
  }

  return ESP_OK;
}

esp_err_t storage::open_file_w() {
  file = fopen(path_file, "w");
  if (file == NULL) {
    ESP_LOGE(TAG, "Failed to open file");
    return ESP_FAIL;
  }
  return ESP_OK;
}
esp_err_t storage::open_file_r() {
  file = fopen(path_file, "r");
  if (file == NULL) {
    ESP_LOGE(TAG, "Failed to open file");
    return ESP_FAIL;
  }
  return ESP_OK;
}
esp_err_t storage::close_file() {
  if (file == NULL) {
    ESP_LOGI(TAG, "File is not open");
    return ESP_FAIL;
  }
  fclose(file);
  return ESP_OK;
}

esp_err_t storage::read_line(char* buffer, size_t len) {
  if (file == NULL) {
    ESP_LOGE(TAG, "Failed to open file for reading");
    return ESP_FAIL;
  }

  if (fgets(buffer, len, file) != NULL) {
  } else if (feof(file)) {
    ESP_LOGI(TAG, "End of file");
    return ESP_FAIL;
  } else {
    ESP_LOGE(TAG, "Failed to read from file");
    return ESP_FAIL;
  }

  ESP_LOGI(TAG, "Read: %s", buffer);

  return ESP_OK;
}

}  // namespace storage