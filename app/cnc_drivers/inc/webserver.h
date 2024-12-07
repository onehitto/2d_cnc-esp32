#pragma once

#include <cstring>
#include <string>
#include "esp_err.h"
#include "esp_log.h"

#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_netif.h"
#include "esp_wifi.h"

#include "nvs.h"
#include "nvs_flash.h"
#include "storage.h"

#include "cJSON.h"

#define RUN_FILE 1
#define EXECUTE_GCODE 2
#define PAUSE 3
#define RESUME 4
#define STOP 5

#define GET_STATUS 6
#define CONFIG 7

namespace network {

struct file_upload_state_t {
  int current_chunk;
  int total_chunks;
};

class webserver {
 public:
  webserver();

  // init webserver (wifi nvs webserver)
  static void webserver_init();

  // wifi setup
  static void wifi_init_ap();
  static void wifi_init_sta();
  static void tsk_restart_as_STA(void* parameter);
  static void tsk_restart_as_AP(void* parameter);

  static void save_credentials();
  static bool process_creadentials_msg(const char* message_buffer);
  static esp_err_t process_ws_message(httpd_req_t* req, const char* message);

  // wifi handler
  static void wifi_event_handler(void* arg,
                                 esp_event_base_t event_base,
                                 int32_t event_id,
                                 void* event_data);
  // webserver
  static void start_webserver(wifi_mode_t mode);
  static esp_err_t stop_webserver();

  // webserver https handlers
  static esp_err_t handle_https_set_credentials(httpd_req_t* req);
  // webserver socket handlers
  static esp_err_t handle_ws_cnc(httpd_req_t* req);

  static esp_err_t ws_send(httpd_handle_t hd,
                           int fd,
                           const char* data,
                           size_t len);
  static void resp_msg(char* buffer,
                       const char* response,
                       const char* err_msg,
                       const char* content);

  static QueueHandle_t queue;
  //  private:
  static char ssid_buffer[32];
  static char password_buffer[64];
  static bool credentials_valid;
  static wifi_config_t wifi_config;
  static httpd_handle_t server;
  static esp_netif_t* wifi_esp_netif;
  static int count_try;
};

}  // namespace network