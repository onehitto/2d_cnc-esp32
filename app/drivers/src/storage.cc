#include "storage.h"
#include <dirent.h>

namespace nm_storage {

#define TAG "STORAGE"
const std::string dir = "/storage/";

FILE* storage::file = NULL;

size_t storage::total = 0, storage::used = 0;

std::list<std::string> storage::files = {};
int storage::file_count = 0;
int storage::line_number = 0;
int storage::total_lines = 0;

std::string storage::file_name_op = "";

storage::storage() {}

esp_err_t storage::init_spiffs() {
  esp_vfs_spiffs_conf_t conf = {.base_path = "/storage",
                                .partition_label = NULL,
                                .max_files = 2,
                                .format_if_mount_failed = true};

  esp_err_t ret = esp_vfs_spiffs_register(&conf);

  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
    return ret;
  }
  ESP_LOGI(TAG, "SPIFFS initialized");
  files = list_files();
  line_number = 0;
  // Check if file exe_g is present and create it if not
  if (std::find(files.begin(), files.end(), "exe_g.txt") == files.end()) {
    create_file("exe_g.txt");
    close_file();
  }
  // Check if file log.txt is present and create it if not
  if (std::find(files.begin(), files.end(), "log.txt") == files.end()) {
    create_file("log.txt");
    close_file();
  }
  ESP_LOGI(TAG, "Files: %d", file_count);
  return ESP_OK;
}

esp_err_t storage::open_file(std::string file_name, const char* mode) {
  if (std::find(files.begin(), files.end(), file_name) == files.end()) {
    ESP_LOGE(TAG, "File not found : %s", file_name.c_str());
    return ESP_FAIL;
  }
  if (file != NULL) {
    ESP_LOGE(TAG, "Failed to open file // there is a File already open");
    return ESP_FAIL;
  }
  std::string path_file = dir + file_name;
  line_number = 0;
  file = fopen(path_file.c_str(), mode);
  if (file == NULL) {
    ESP_LOGE(TAG, "Failed to open file : %s", file_name.c_str());
    return ESP_FAIL;
  }
  file_name_op = file_name;
  if (mode[0] == 'r')
    num_lines();
  return ESP_OK;
}

esp_err_t storage::close_file() {
  if (file == NULL) {
    ESP_LOGI(TAG, "File is not open");
    return ESP_FAIL;
  }
  fclose(file);
  file_name_op = "";
  file = NULL;
  return ESP_OK;
}

esp_err_t storage::read_line(char* buffer, size_t len) {
  if (file == NULL) {
    ESP_LOGE(TAG, "Failed to open file for reading // there is no file open");
    return ESP_FAIL;
  }

  if (fgets(buffer, len, file) != NULL) {
  } else if (feof(file)) {
    ESP_LOGI(TAG, "End of file");
    return ESP_ERR_INVALID_SIZE;
  } else {
    ESP_LOGE(TAG, "Failed to read from file : %s", file_name_op.c_str());
    return ESP_FAIL;
  }
  line_number++;
  return ESP_OK;
}

esp_err_t storage::write_file(const char* content) {
  if (file == NULL) {
    ESP_LOGE(TAG, "Failed to open file for writing");
    return ESP_FAIL;
  }
  if (fprintf(file, "%s", content) < 0) {
    ESP_LOGE(TAG, "Failed to write in file : %s", file_name_op.c_str());
    return ESP_FAIL;
  }
  line_number++;
  return ESP_OK;
}

std::list<std::string> storage::list_files() {
  file_count = 0;
  std::list<std::string> p_files = {};
  DIR* dir = opendir("/storage");
  struct dirent* entry;
  while ((entry = readdir(dir)) != NULL) {
    ESP_LOGI(TAG, "File: %s", entry->d_name);
    p_files.push_back(entry->d_name);
    file_count++;
  }
  closedir(dir);
  files = p_files;
  return p_files;
}

esp_err_t storage::create_file(std::string file_name) {
  if (std::find(files.begin(), files.end(), file_name) != files.end()) {
    ESP_LOGE(TAG, "File already exists : %s", file_name.c_str());
    return ESP_FAIL;
  }
  std::string path_file = dir + file_name;

  file = fopen(path_file.c_str(), "w");

  if (file == NULL) {
    ESP_LOGE(TAG, "Failed to create file : %s", file_name.c_str());
    return ESP_FAIL;
  }
  file_count++;
  file_name_op = file_name;
  line_number = 0;
  ESP_LOGI(TAG, "File created : %s", file_name.c_str());
  return ESP_OK;
}

esp_err_t storage::delete_file(std::string file_name) {
  if (std::find(files.begin(), files.end(), file_name) == files.end()) {
    ESP_LOGE(TAG, "File not found : %s", file_name.c_str());
    return ESP_FAIL;
  }
  if (file != NULL && file_name_op == file_name) {
    ESP_LOGE(TAG, "cannot delete a open file");
    return ESP_FAIL;
  }
  std::string path_file = dir + file_name;
  if (remove(path_file.c_str()) != 0) {
    ESP_LOGE(TAG, "Failed to delete file : %s", file_name.c_str());
    return ESP_FAIL;
  }
  file_count--;
  files.remove(file_name);
  return ESP_OK;
}

size_t storage::Remaing_space() {
  esp_spiffs_info(NULL, &total, &used);
  ESP_LOGI(TAG, "Total: %d, Used: %d", total, used);
  return total - used;
}

void storage::num_lines() {
  if (file == NULL) {
    ESP_LOGE(TAG, "Failed to open file for reading // there is no file open");
    return;
  }
  fseek(file, 0, SEEK_SET);  // Reset the cursor to the first line
  int counter = 0;
  char buffer[100];
  while (fgets(buffer, 100, file) != NULL) {
    counter++;
  }
  total_lines = counter;
  ESP_LOGI(TAG, "Number of lines: %d", counter);
  fseek(file, 0, SEEK_SET);  // Reset the cursor to the first line
}

}  // namespace nm_storage

/*
//test :

#include "main.h"

#define TAG "MAIN"

using namespace nm_storage;

extern "C" void app_main(void) {
  // engine::cnc obj;
  // network::webserver ws;

  // ws.webserver_init();

  ESP_ERROR_CHECK(storage::init_spiffs());

  storage::create_file("file1.txt");
  storage::write_file("Hello World file 1");
  vTaskDelay(pdSECOND * 1);
  storage::close_file();

  storage::create_file("file2.txt");
  storage::write_file("Hello World file 2");
  vTaskDelay(pdSECOND * 1);
  storage::close_file();

  storage::create_file("file3.txt");
  storage::write_file("Hello World file 3");
  vTaskDelay(pdSECOND * 1);
  storage::close_file();

  while (1) {
    ESP_LOGI(TAG, "Hello cnc !!");
    ESP_LOGI(TAG, "Files: %d", storage::file_count);
    for (const auto& file_name : storage::list_files()) {
      ESP_LOGI(TAG, "File: %s", file_name.c_str());
      if (storage::open_file(file_name.c_str(), "r") == ESP_OK) {
        char buffer[100];
        storage::read_line(buffer, 100);
        ESP_LOGI(TAG, "Content: %s", buffer);
        storage::close_file();
      }
      vTaskDelay(pdSECOND * 5);
    }
    storage::delete_file("file1.txt");
    vTaskDelay(pdSECOND * 1);
  }
}
*/