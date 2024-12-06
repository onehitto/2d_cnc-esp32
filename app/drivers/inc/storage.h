#pragma once

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <list>
#include <string>
#include "esp_err.h"
#include "esp_log.h"
#include "esp_spiffs.h"

namespace nm_storage {

class storage {
 public:
  storage();

  static esp_err_t init_spiffs();

  static esp_err_t open_file(std::string file_name, const char* mode);
  static esp_err_t close_file();

  static esp_err_t read_line(char* buffer, size_t len);
  static esp_err_t write_file(const char* content);

  static std::list<std::string> list_files();
  static esp_err_t create_file(std::string file_name);
  static esp_err_t delete_file(std::string file_name);
  static size_t Remaing_space();
  static void num_lines();

  static size_t total;
  static size_t used;

  static std::list<std::string> files;
  static int line_number;
  static int total_lines;
  static int file_count;
  static std::string file_name_op;
  static FILE* file;
};

}  // namespace nm_storage