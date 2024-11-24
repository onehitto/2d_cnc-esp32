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

#include "cJSON.h"

namespace network {

class webserver {
 public:
  webserver();

  void wifi_init_ap();
  void wifi_init_sta();

  void webserver_init();
  static void start_webserver(wifi_mode_t mode);
  static esp_err_t stop_webserver();
  // void save_credentials(const char* ssid, const char* password);
  // void webserver_event_handler(void* arg, esp_websocket_event_data_t*
  // event);

  static esp_err_t handle_ws_setcreadentials(httpd_req_t* req);
  static void process_creadentials_msg(std::string& message_buffer);
  static void wifi_event_handler(void* arg,
                                 esp_event_base_t event_base,
                                 int32_t event_id,
                                 void* event_data);

  //  private:
  static char ssid_buffer[32];
  static char password_buffer[64];
};

// static const httpd_uri_t uri_get = {.uri = "/",
//                                     .method = HTTP_GET,
//                                     .handler = get_req_handler,
//                                     .user_ctx = NULL};

}  // namespace network