#pragma once

#include <cstddef>
#include <cstring>
#include <string>
#include "esp_err.h"
#include "esp_log.h"
#include "esp_spiffs.h"

namespace storage {

class storage {
 public:
  storage();

  static void init_spiffs();
  static size_t Remaing_space();

  static esp_err_t open_file_r();
  static esp_err_t open_file_w();
  static esp_err_t close_file();

  static esp_err_t read_line(char* buffer, size_t len);
  static esp_err_t write_file(const char* content);

  static esp_vfs_spiffs_conf_t conf;
  static size_t total;
  static size_t used;

  static const char* path_file;
  static FILE* file;
};

// static const httpd_uri_t uri_get = {.uri = "/",
//                                     .method = HTTP_GET,
//                                     .handler = get_req_handler,
//                                     .user_ctx = NULL};

}  // namespace storage