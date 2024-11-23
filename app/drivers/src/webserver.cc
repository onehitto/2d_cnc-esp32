#include "webserver.h"

namespace network {

#define TAG "WEB_SERVER"

static wifi_config_t wifi_config = {};
static httpd_handle_t server = NULL;
static int count_try = 0;

webserver::webserver() {
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
      ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);

  ret = nvs_open("wifi_config", NVS_READONLY, &nvs_handle);
  if (ret == ESP_OK) {
    size_t ssid_len = sizeof(ssid_buffer);
    size_t password_len = sizeof(password_buffer);

    if (nvs_get_str(nvs_handle, "ssid", ssid_buffer, &ssid_len) == ESP_OK &&
        nvs_get_str(nvs_handle, "password", password_buffer, &password_len) ==
            ESP_OK) {
      ESP_LOGI(TAG, "Found stored Wi-Fi credentials. SSID: %s", ssid_buffer);
      nvs_close(nvs_handle);
      wifi_init_sta();  // Try to connect in STA mode
    } else {
      ESP_LOGI(TAG, "No Wi-Fi credentials found. Starting in AP mode.");
      nvs_close(nvs_handle);
      wifi_init_ap();  // Start in AP mode
    }
  } else {
    ESP_LOGI(TAG, "Failed to open NVS. Starting in AP mode.");
    wifi_init_ap();  // Start in AP mode
  }
}

void webserver::wifi_init_sta() {
  ESP_LOGI(TAG, "Initializing Wi-Fi in STA mode...");

  // esp_netif_init();
  // esp_event_loop_create_default();
  // esp_netif_create_default_wifi_sta();

  // wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  // ESP_ERROR_CHECK(esp_wifi_init(&cfg));

  // // Register event handlers
  // esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
  //                                     &wifi_event_handler, this, nullptr);
  // esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
  //                                     &wifi_event_handler, this, nullptr);

  // wifi_config_t wifi_config = {};
  // strncpy((char*)wifi_config.sta.ssid, ssid_buffer,
  //         sizeof(wifi_config.sta.ssid));
  // strncpy((char*)wifi_config.sta.password, password_buffer,
  //         sizeof(wifi_config.sta.password));
  // wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

  // ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  // ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
  // ESP_ERROR_CHECK(esp_wifi_start());

  // ESP_LOGI(TAG, "STA mode initialization complete.");
}

void webserver::wifi_init_ap() {
  network::count_try = 0;
  ESP_LOGI(TAG, "Initializing Wi-Fi in AP mode...");

  esp_netif_init();
  esp_event_loop_create_default();
  esp_netif_create_default_wifi_ap();

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  esp_wifi_init(&cfg);

  wifi_config = {};
  strncpy((char*)wifi_config.ap.ssid, "2d_cnc_esp32",
          sizeof(wifi_config.ap.ssid));
  wifi_config.ap.ssid_len = strlen("2d_cnc_esp32");
  wifi_config.ap.max_connection = 4;
  wifi_config.ap.authmode = WIFI_AUTH_OPEN;

  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));

  esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                      &wifi_event_handler, nullptr, nullptr);
  esp_event_handler_instance_register(IP_EVENT, IP_EVENT_AP_STAIPASSIGNED,
                                      &wifi_event_handler, nullptr, nullptr);

  ESP_ERROR_CHECK(esp_wifi_start());

  ESP_LOGI(TAG, "AP mode initialized. SSID: %s", wifi_config.ap.ssid);

  start_webserver();
}

void webserver::start_webserver() {
  /* Generate default configuration */
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  server = NULL;
  config.server_port = 8080;
  config.lru_purge_enable = true;

  /* Start the httpd server */
  if (httpd_start(&server, &config) == ESP_OK) {
    /* Register URI handlers */
    ESP_LOGI(TAG, "Registering URI handlers");
    httpd_register_uri_handler(server, &network::hello);
    return;
  }
  ESP_LOGI(TAG, "Error starting server!");
}

esp_err_t webserver::stop_webserver() {
  // Stop the httpd server
  return httpd_stop(network::server);
}

esp_err_t webserver::hello_get_handler(httpd_req_t* req) {
  const char* resp_str = (const char*)req->user_ctx;
  ESP_LOGI(TAG, "Sending response: %s", resp_str);
  httpd_resp_send(req, resp_str, strlen(resp_str));
  return ESP_OK;
}

void webserver::wifi_event_handler(void* arg,
                                   esp_event_base_t event_base,
                                   int32_t event_id,
                                   void* event_data) {
  if (event_base == WIFI_EVENT) {
    if (event_id == WIFI_EVENT_AP_START) {
      ESP_LOGI(TAG, "Connecting to Wi-Fi...");
      esp_wifi_connect();
    }  // else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
    //   ESP_LOGW(TAG, "Wi-Fi connection failed. Switching to AP mode.");
    //   if (count_try == 3) {
    //     webserver::stop_webserver();
    //     esp_wifi_stop();
    //     wifi_init_ap();
    //     count_try = 0;
    //   }
    //   count_try++;
    // }
  }
}

// void webserver::save_credentials(const char* ssid, const char* password) {
//   nvs_handle_t nvs_handle;
//   ESP_ERROR_CHECK(nvs_open("wifi_config", NVS_READWRITE, &nvs_handle));

//   ESP_ERROR_CHECK(nvs_set_str(nvs_handle, "ssid", ssid));
//   ESP_ERROR_CHECK(nvs_set_str(nvs_handle, "password", password));
//   ESP_ERROR_CHECK(nvs_commit(nvs_handle));

//   nvs_close(nvs_handle);
//   ESP_LOGI(TAG, "Saved Wi-Fi credentials: SSID=%s", ssid);

//   // Stop AP mode
//   esp_wifi_stop();

//   // Switch to STA mode and attempt to connect
//   strncpy(ssid_buffer, ssid, sizeof(ssid_buffer));
//   strncpy(password_buffer, password, sizeof(password_buffer));
//   wifi_init_sta();
// }
}  // namespace network