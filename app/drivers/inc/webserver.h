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

namespace network {

class webserver {
 public:
  webserver();

  static void wifi_init_ap();
  static void wifi_init_sta();

  static void start_webserver();
  static esp_err_t stop_webserver();
  // void save_credentials(const char* ssid, const char* password);
  // void webserver_event_handler(void* arg, esp_websocket_event_data_t*
  // event);
  static esp_err_t hello_get_handler(httpd_req_t* req);
  static void wifi_event_handler(void* arg,
                                 esp_event_base_t event_base,
                                 int32_t event_id,
                                 void* event_data);

  char ssid_buffer[32];
  char password_buffer[64];

  nvs_handle_t nvs_handle;
};

static const httpd_uri_t hello = {.uri = "/hello",
                                  .method = HTTP_GET,
                                  .handler = webserver::hello_get_handler,
                                  /* Let's pass response string in user
                                   * context to demonstrate it's usage */
                                  .user_ctx = (void*)"Hello, world!"};
}  // namespace network