#include "webserver.h"

namespace network {

#define TAG "WEB_SERVER"

// global variables
wifi_config_t webserver::wifi_config = {};
httpd_handle_t webserver::server = NULL;
esp_netif_t* webserver::wifi_esp_netif = NULL;

static esp_event_handler_instance_t instance_any_id;
static esp_event_handler_instance_t instance_got_ip;

int webserver::count_try = 0;

char webserver::ssid_buffer[32] = {0};  // Initialize with null characters
char webserver::password_buffer[64] = {0};
bool webserver::credentials_valid = false;

// WebSocket frames
httpd_ws_frame_t pong_pkt = {.final = true,
                             .fragmented = false,
                             .type = HTTPD_WS_TYPE_PONG,
                             .payload = nullptr,
                             .len = 0};

httpd_ws_frame_t close_frame = {
    .final = true,
    .fragmented = false,
    .type = HTTPD_WS_TYPE_CLOSE,  // WebSocket close frame type
    .payload = NULL,              // No payload for a simple close frame
    .len = 0,                     // Length of the payload
};
httpd_ws_frame_t hello_frame = {.final = true,
                                .fragmented = false,
                                .type = HTTPD_WS_TYPE_TEXT,
                                .payload = nullptr,
                                .len = 0};

// websocket uri handlers
static httpd_uri_t ws_setcreadentials = {
    .uri = "/ws_setcreadentials",
    .method = HTTP_GET,
    .handler = webserver::handle_ws_setcreadentials,
    .user_ctx = NULL,
    .is_websocket = true,
    .handle_ws_control_frames = true,
    .supported_subprotocol = NULL};

static httpd_uri_t ws_hello = {.uri = "/ws_hello",
                               .method = HTTP_GET,
                               .handler = webserver::handle_ws_hello,
                               .user_ctx = NULL,
                               .is_websocket = true,
                               .handle_ws_control_frames = true,
                               .supported_subprotocol = NULL};

static httpd_uri_t ws_filesys = {.uri = "/ws_filesys",
                                 .method = HTTP_GET,
                                 .handler = webserver::handle_ws_filesys,
                                 .user_ctx = NULL,
                                 .is_websocket = true,
                                 .handle_ws_control_frames = true,
                                 .supported_subprotocol = NULL};

webserver::webserver() {}

void webserver::webserver_init() {
  esp_netif_init();
  esp_event_loop_create_default();

  // 1- check if the NVS partition has been initialized
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
      ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);

  nvs_handle_t nvs_handle;
  // 2 - check if the Wi - Fi credentials are stored in NVS ret =
  nvs_open("wifi_config", NVS_READONLY, &nvs_handle);
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
  count_try = 0;
  ESP_LOGI(TAG, "Initializing Wi-Fi in STA mode...");

  wifi_esp_netif = esp_netif_create_default_wifi_sta();

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));

  // Register event handlers
  esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                      &wifi_event_handler, nullptr,
                                      &instance_any_id);
  esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                      &wifi_event_handler, nullptr,
                                      &instance_got_ip);

  wifi_config_t wifi_config = {};
  strncpy((char*)wifi_config.sta.ssid, ssid_buffer,
          sizeof(wifi_config.sta.ssid));
  strncpy((char*)wifi_config.sta.password, password_buffer,
          sizeof(wifi_config.sta.password));
  wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
  ESP_ERROR_CHECK(esp_wifi_start());

  ESP_LOGI(TAG, "STA mode initialization complete.");
}

void webserver::wifi_init_ap() {
  ESP_LOGI(TAG, "Initializing Wi-Fi in AP mode...");

  wifi_esp_netif = esp_netif_create_default_wifi_ap();

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));

  wifi_config = {};
  strncpy((char*)wifi_config.ap.ssid, "2d_cnc_esp32",
          sizeof(wifi_config.ap.ssid));
  wifi_config.ap.ssid_len = strlen("2d_cnc_esp32");
  wifi_config.ap.max_connection = 4;
  wifi_config.ap.authmode = WIFI_AUTH_OPEN;
  wifi_config.ap.ssid_hidden = 0;
  wifi_config.ap.beacon_interval = 100;
  wifi_config.ap.channel = 1;

  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));

  ESP_ERROR_CHECK(esp_event_handler_instance_register(
      WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, nullptr,
      &instance_any_id));

  ESP_ERROR_CHECK(esp_wifi_start());

  ESP_LOGI(TAG, "AP mode initialized. SSID: %s", wifi_config.ap.ssid);
}

void webserver::start_webserver(wifi_mode_t mode) {
  /* Generate default configuration */
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.lru_purge_enable = true;
  if (server != NULL) {
    ESP_LOGW(TAG, "Web server is already running");
    return;
  }

  if (mode == WIFI_MODE_AP) {
    config.server_port = 4397;  // Port for AP mode
  } else {
    config.server_port = 4398;  // Port for STA mode
  }
  ESP_LOGI(TAG, "Web server starting on port: %d", config.server_port);
  /* Start the httpd server */
  if (httpd_start(&server, &config) == ESP_OK) {
    /* Register URI handlers */
    ESP_LOGI(TAG, "Registering URI handlers");
    if (mode == WIFI_MODE_AP)
      httpd_register_uri_handler(server, &ws_setcreadentials);
    else {
      httpd_register_uri_handler(server, &ws_hello);
      httpd_register_uri_handler(server, &ws_filesys);
    }
    return;
  }
  ESP_LOGI(TAG, "Error starting server!");
}

esp_err_t webserver::stop_webserver() {
  // Stop the httpd server
  if (server == NULL) {
    ESP_LOGW(TAG, "Web server is not running");
    return ESP_OK;
  }
  ESP_LOGI(TAG, "Stopping webserver...");
  esp_err_t ret = httpd_stop(server);
  if (ret == ESP_OK) {
    server = NULL;
  } else {
    ESP_LOGE(TAG, "Failed to stop webserver: %s", esp_err_to_name(ret));
  }
  return ret;
}

std::string message_buffer;

esp_err_t webserver::handle_ws_setcreadentials(httpd_req_t* req) {
  if (req->method == HTTP_GET) {
    ESP_LOGD(TAG, "Handshake done, the new connection was opened");
    return ESP_OK;
  }

  httpd_ws_frame_t ws_pkt;
  memset(&ws_pkt, 0, sizeof(ws_pkt));

  // Get metadata for the WebSocket frame
  esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, 0);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to get WebSocket frame size: %s",
             esp_err_to_name(ret));
    return ret;
  }

  ESP_LOGD(TAG, "WebSocket frame type: %d, length: %d, final: %d", ws_pkt.type,
           ws_pkt.len, ws_pkt.final);

  // Allocate memory for the payload
  if (ws_pkt.len > 0) {
    ws_pkt.payload =
        (uint8_t*)malloc(ws_pkt.len + 1);  // +1 for null termination
    if (!ws_pkt.payload) {
      ESP_LOGE(TAG, "Failed to allocate memory for WebSocket payload");
      return ESP_ERR_NO_MEM;
    }

    // Receive the payload
    int retries = 3;
    while (retries-- > 0) {
      ret = httpd_ws_recv_frame(req, &ws_pkt, ws_pkt.len);
      if (ret == ESP_OK)
        break;  // Success
      if (ret == ESP_ERR_INVALID_STATE || ret == ESP_ERR_NO_MEM) {
        ESP_LOGE(TAG, "WebSocket recv error: %s", esp_err_to_name(ret));
        free(ws_pkt.payload);
        return ret;
      }
      ESP_LOGW(TAG, "Retrying WebSocket recv...");
      vTaskDelay(pdMS_TO_TICKS(50));  // Wait 50ms before retrying
    }
    if (retries <= 0) {
      ESP_LOGE(TAG, "Failed to receive WebSocket payload after retries");
      free(ws_pkt.payload);
      return ESP_FAIL;
    }
    ws_pkt.payload[ws_pkt.len] = '\0';  // Null-terminate for safety
  }

  // Handle frame types
  switch (ws_pkt.type) {
    case HTTPD_WS_TYPE_CONTINUE:
      ESP_LOGD(TAG, "Continuation WebSocket frame received");
      message_buffer += (char*)ws_pkt.payload;  // Append to the buffer
      break;
    case HTTPD_WS_TYPE_TEXT:
      ESP_LOGD(TAG, "Text WebSocket frame received");
      message_buffer = (char*)ws_pkt.payload;  // Start a new message
      break;

    case HTTPD_WS_TYPE_BINARY:
      ESP_LOGD(TAG, "Binary WebSocket frame received");
      ESP_LOG_BUFFER_HEX(TAG, ws_pkt.payload, ws_pkt.len);
      break;

    case HTTPD_WS_TYPE_CLOSE:
      ESP_LOGD(TAG, "Close WebSocket frame received. Cleaning up.");
      free(ws_pkt.payload);
      if (credentials_valid) {
        save_credentials();
        if (xTaskCreate(restart_as_STA, "restart_as_STA", 4096, NULL, 5,
                        NULL) != pdPASS) {
          ESP_LOGE(TAG, "Failed to create server stop task.");
          return ESP_ERR_NO_MEM;
        }
      }
      if (httpd_ws_send_frame(req, &close_frame) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to send WebSocket close frame: %s",
                 esp_err_to_name(ret));
      } else {
        ESP_LOGI(TAG, "WebSocket close frame sent successfully.");
      }
      return ESP_OK;

    case HTTPD_WS_TYPE_PING:
      ESP_LOGD(TAG, "Ping WebSocket frame received");
      pong_pkt.payload = ws_pkt.payload;
      pong_pkt.len = ws_pkt.len;
      httpd_ws_send_frame(req, &pong_pkt);
      break;

    case HTTPD_WS_TYPE_PONG:
      ESP_LOGD(TAG, "Pong WebSocket frame received");
      break;

    default:
      ESP_LOGW(TAG, "Unsupported WebSocket frame type: %d", ws_pkt.type);
      free(ws_pkt.payload);
      return ESP_ERR_INVALID_ARG;
  }

  // If this is the final frame, process the full message
  if (ws_pkt.final) {
    ESP_LOGD(TAG, "Full WebSocket message received: %s",
             message_buffer.c_str());
    // Process the message here
    process_creadentials_msg(message_buffer);
    message_buffer.clear();  // Clear buffer for the next message
  }

  // Free the payload memory
  if (ws_pkt.payload) {
    free(ws_pkt.payload);
  }

  return ESP_OK;
}

esp_err_t webserver::handle_ws_hello(httpd_req_t* req) {
  if (req->method == HTTP_GET) {
    ESP_LOGD(TAG, "Handshake done, the new connection was opened");
    return ESP_OK;
  }

  httpd_ws_frame_t ws_pkt;
  memset(&ws_pkt, 0, sizeof(ws_pkt));

  // Get metadata for the WebSocket frame
  esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, 0);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to get WebSocket frame size: %s",
             esp_err_to_name(ret));
    return ret;
  }

  ESP_LOGD(TAG, "WebSocket frame type: %d, length: %d, final: %d", ws_pkt.type,
           ws_pkt.len, ws_pkt.final);

  // Allocate memory for the payload
  if (ws_pkt.len > 0) {
    ws_pkt.payload =
        (uint8_t*)malloc(ws_pkt.len + 1);  // +1 for null termination
    if (!ws_pkt.payload) {
      ESP_LOGE(TAG, "Failed to allocate memory for WebSocket payload");
      return ESP_ERR_NO_MEM;
    }

    // Receive the payload
    int retries = 3;
    while (retries-- > 0) {
      ret = httpd_ws_recv_frame(req, &ws_pkt, ws_pkt.len);
      if (ret == ESP_OK)
        break;  // Success
      if (ret == ESP_ERR_INVALID_STATE || ret == ESP_ERR_NO_MEM) {
        ESP_LOGE(TAG, "WebSocket recv error: %s", esp_err_to_name(ret));
        free(ws_pkt.payload);
        return ret;
      }
      ESP_LOGW(TAG, "Retrying WebSocket recv...");
      vTaskDelay(pdMS_TO_TICKS(50));  // Wait 50ms before retrying
    }
    if (retries <= 0) {
      ESP_LOGE(TAG, "Failed to receive WebSocket payload after retries");
      free(ws_pkt.payload);
      return ESP_FAIL;
    }
    ws_pkt.payload[ws_pkt.len] = '\0';  // Null-terminate for safety
  }

  // Handle frame types
  switch (ws_pkt.type) {
    case HTTPD_WS_TYPE_CONTINUE:
      ESP_LOGD(TAG, "Continuation WebSocket frame received");
      message_buffer += (char*)ws_pkt.payload;  // Append to the buffer
      break;
    case HTTPD_WS_TYPE_TEXT:
      ESP_LOGD(TAG, "Text WebSocket frame received");
      message_buffer = (char*)ws_pkt.payload;  // Start a new message
      break;

    case HTTPD_WS_TYPE_BINARY:
      ESP_LOGD(TAG, "Binary WebSocket frame received");
      ESP_LOG_BUFFER_HEX(TAG, ws_pkt.payload, ws_pkt.len);
      break;

    case HTTPD_WS_TYPE_CLOSE:
      ESP_LOGD(TAG, "Close WebSocket frame received. Cleaning up.");
      free(ws_pkt.payload);
      if (httpd_ws_send_frame(req, &close_frame) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to send WebSocket close frame: %s",
                 esp_err_to_name(ret));
      } else {
        ESP_LOGI(TAG, "WebSocket close frame sent successfully.");
      }
      return ESP_OK;

    case HTTPD_WS_TYPE_PING:
      ESP_LOGD(TAG, "Ping WebSocket frame received");
      pong_pkt.payload = ws_pkt.payload;
      pong_pkt.len = ws_pkt.len;
      httpd_ws_send_frame(req, &pong_pkt);
      break;

    case HTTPD_WS_TYPE_PONG:
      ESP_LOGD(TAG, "Pong WebSocket frame received");
      break;

    default:
      ESP_LOGW(TAG, "Unsupported WebSocket frame type: %d", ws_pkt.type);
      free(ws_pkt.payload);
      return ESP_ERR_INVALID_ARG;
  }

  // If this is the final frame, process the full message
  if (ws_pkt.final) {
    ESP_LOGD(TAG, "Full WebSocket message received: %s",
             message_buffer.c_str());
    // Process the message here
    message_buffer.clear();  // Clear buffer for the next message
  }

  const char* response = "Hello world from ESP32 WebSocket server!";

  hello_frame.payload = (uint8_t*)response;
  hello_frame.len = strlen(response);

  if (httpd_ws_send_frame(req, &hello_frame) != ESP_OK) {
    ESP_LOGE(TAG, "Failed to send WebSocket response: %s",
             esp_err_to_name(ret));
  } else {
    ESP_LOGI(TAG, "Sent message: %s", response);
  }

  // Free the payload memory
  if (ws_pkt.payload) {
    free(ws_pkt.payload);
  }

  return ESP_OK;
}

esp_err_t webserver::handle_ws_filesys(httpd_req_t* req) {
  if (req->method == HTTP_GET) {
    ESP_LOGD(TAG, "Handshake done, the new connection was opened");
    return ESP_OK;
  }

  httpd_ws_frame_t ws_pkt;
  memset(&ws_pkt, 0, sizeof(ws_pkt));

  // Get metadata for the WebSocket frame
  esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, 0);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to get WebSocket frame size: %s",
             esp_err_to_name(ret));
    return ret;
  }

  ESP_LOGD(TAG, "WebSocket frame type: %d, length: %d, final: %d", ws_pkt.type,
           ws_pkt.len, ws_pkt.final);

  // Allocate memory for the payload
  if (ws_pkt.len > 0) {
    ws_pkt.payload =
        (uint8_t*)malloc(ws_pkt.len + 1);  // +1 for null termination
    if (!ws_pkt.payload) {
      ESP_LOGE(TAG, "Failed to allocate memory for WebSocket payload");
      return ESP_ERR_NO_MEM;
    }

    // Receive the payload
    int retries = 3;
    while (retries-- > 0) {
      ret = httpd_ws_recv_frame(req, &ws_pkt, ws_pkt.len);
      if (ret == ESP_OK)
        break;  // Success
      if (ret == ESP_ERR_INVALID_STATE || ret == ESP_ERR_NO_MEM) {
        ESP_LOGE(TAG, "WebSocket recv error: %s", esp_err_to_name(ret));
        free(ws_pkt.payload);
        return ret;
      }
      ESP_LOGW(TAG, "Retrying WebSocket recv...");
      vTaskDelay(pdMS_TO_TICKS(50));  // Wait 50ms before retrying
    }
    if (retries <= 0) {
      ESP_LOGE(TAG, "Failed to receive WebSocket payload after retries");
      free(ws_pkt.payload);
      return ESP_FAIL;
    }
    ws_pkt.payload[ws_pkt.len] = '\0';  // Null-terminate for safety
  }

  // Handle frame types
  switch (ws_pkt.type) {
    case HTTPD_WS_TYPE_CONTINUE:
      ESP_LOGD(TAG, "Continuation WebSocket frame received");
      message_buffer += (char*)ws_pkt.payload;  // Append to the buffer
      break;
    case HTTPD_WS_TYPE_TEXT:
      ESP_LOGD(TAG, "Text WebSocket frame received");
      message_buffer = (char*)ws_pkt.payload;  // Start a new message
      break;

    case HTTPD_WS_TYPE_BINARY:
      ESP_LOGD(TAG, "Binary WebSocket frame received");
      ESP_LOG_BUFFER_HEX(TAG, ws_pkt.payload, ws_pkt.len);
      break;

    case HTTPD_WS_TYPE_CLOSE:
      ESP_LOGD(TAG, "Close WebSocket frame received. Cleaning up.");
      free(ws_pkt.payload);
      if (httpd_ws_send_frame(req, &close_frame) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to send WebSocket close frame: %s",
                 esp_err_to_name(ret));
      } else {
        ESP_LOGI(TAG, "WebSocket close frame sent successfully.");
      }
      return ESP_OK;

    case HTTPD_WS_TYPE_PING:
      ESP_LOGD(TAG, "Ping WebSocket frame received");
      pong_pkt.payload = ws_pkt.payload;
      pong_pkt.len = ws_pkt.len;
      httpd_ws_send_frame(req, &pong_pkt);
      break;

    case HTTPD_WS_TYPE_PONG:
      ESP_LOGD(TAG, "Pong WebSocket frame received");
      break;

    default:
      ESP_LOGW(TAG, "Unsupported WebSocket frame type: %d", ws_pkt.type);
      free(ws_pkt.payload);
      return ESP_ERR_INVALID_ARG;
  }

  // If this is the final frame, process the full message
  if (ws_pkt.final) {
    ESP_LOGI(TAG, "Full WebSocket message received: %s",
             message_buffer.c_str());
    // Process the message here
    message_buffer.clear();  // Clear buffer for the next message
  }

  const char* response = "Hello world from ESP32 WebSocket server!";

  hello_frame.payload = (uint8_t*)response;
  hello_frame.len = strlen(response);

  if (httpd_ws_send_frame(req, &hello_frame) != ESP_OK) {
    ESP_LOGE(TAG, "Failed to send WebSocket response: %s",
             esp_err_to_name(ret));
  } else {
    ESP_LOGI(TAG, "Sent message: %s", response);
  }

  // Free the payload memory
  if (ws_pkt.payload) {
    free(ws_pkt.payload);
  }

  return ESP_OK;
}

void webserver::process_creadentials_msg(std::string& message_buffer) {
  bool ssid_received = false;
  bool password_received = false;
  cJSON* root = cJSON_Parse(message_buffer.c_str());
  if (root == nullptr) {
    ESP_LOGE(TAG, "Failed to parse JSON");
  } else {
    // Extract "ssid"
    cJSON* ssid_json = cJSON_GetObjectItem(root, "ssid");
    if (cJSON_IsString(ssid_json) && (ssid_json->valuestring != nullptr)) {
      snprintf(ssid_buffer, sizeof(ssid_buffer), "%s",
               ssid_json->valuestring);  // Assign value to the variable
      ssid_received = true;
      ESP_LOGI(TAG, "SSID: %s", ssid_buffer);
    } else {
      ESP_LOGW(TAG, "SSID key is missing or invalid");
    }

    // Extract "password"
    cJSON* password_json = cJSON_GetObjectItem(root, "password");
    if (cJSON_IsString(password_json) &&
        (password_json->valuestring != nullptr)) {
      snprintf(password_buffer, sizeof(password_buffer), "%s",
               password_json->valuestring);  // Assign value to the variable
      ESP_LOGI(TAG, "Password: %s", password_buffer);
      password_received = true;
    } else {
      ESP_LOGW(TAG, "Password key is missing or invalid");
    }
    credentials_valid = ssid_received && password_received;
    if (credentials_valid) {
      ESP_LOGI(TAG, "Both SSID and Password were received and are valid.");
    } else {
      ESP_LOGW(TAG, "Incomplete credentials: SSID or Password is missing.");
    }
    // Free the JSON object
    cJSON_Delete(root);
  }
}

void webserver::wifi_event_handler(void* arg,
                                   esp_event_base_t event_base,
                                   int32_t event_id,
                                   void* event_data) {
  if (event_base == WIFI_EVENT) {
    if (event_id == WIFI_EVENT_AP_START) {
      ESP_LOGI(TAG, "Wi-Fi started in AP mode");
      start_webserver(WIFI_MODE_AP);
      // esp_wifi_connect();
    } else if (event_id == WIFI_EVENT_STA_START) {
      ESP_LOGI(TAG, "Wi-Fi started, attempting to connect...");
      esp_wifi_connect();
    } else if (event_id == WIFI_EVENT_STA_DISCONNECTED) {
      ESP_LOGW(TAG, "Disconnected from Wi-Fi, retrying...");
      if (count_try < 3) {
        count_try++;
        esp_wifi_connect();
        ESP_LOGI(TAG, "Retrying Wi-Fi connection (%d/3)...", count_try);
        vTaskDelay(pdMS_TO_TICKS(100));
      } else {
        ESP_LOGE(
            TAG,
            "Failed to connect after 3 attempts. Falling back to AP mode.");
        ESP_LOGI(TAG, "Stopping Wi-Fi...");
        esp_wifi_stop();
        ESP_LOGI(TAG, "Destroying default Wi-Fi interface...");
        esp_netif_destroy_default_wifi(wifi_esp_netif);
        ESP_ERROR_CHECK(esp_event_handler_instance_unregister(
            WIFI_EVENT, ESP_EVENT_ANY_ID, instance_any_id));
        ESP_ERROR_CHECK(esp_event_handler_instance_unregister(
            WIFI_EVENT, IP_EVENT_STA_GOT_IP, instance_got_ip));
        ESP_LOGI(TAG, "Unregistered Wi-Fi event handlers.");
        wifi_init_ap();  // Switch to AP mode
      }
    } else if (event_id == WIFI_EVENT_STA_CONNECTED) {
      wifi_event_sta_connected_t* event =
          (wifi_event_sta_connected_t*)event_data;
      ESP_LOGI(TAG, "Connected to Wi-Fi. SSID: %s", event->ssid);
    } else {
      ESP_LOGI(TAG, "EVENT %ld ", event_id);
    }
  } else if (event_base == IP_EVENT) {
    if (event_id == IP_EVENT_STA_GOT_IP) {
      ip_event_got_ip_t* event = (ip_event_got_ip_t*)event_data;
      ESP_LOGI(TAG, "Got IP address: " IPSTR, IP2STR(&event->ip_info.ip));
      start_webserver(WIFI_MODE_STA);
    }
  }
}

void webserver::save_credentials() {
  nvs_handle_t nvs_handle;
  ESP_ERROR_CHECK(nvs_open("wifi_config", NVS_READWRITE, &nvs_handle));

  ESP_ERROR_CHECK(nvs_set_str(nvs_handle, "ssid", ssid_buffer));
  ESP_ERROR_CHECK(nvs_set_str(nvs_handle, "password", password_buffer));
  ESP_ERROR_CHECK(nvs_commit(nvs_handle));

  nvs_close(nvs_handle);
  ESP_LOGI(TAG, "Saved Wi-Fi credentials: SSID=%s", ssid_buffer);
}

void webserver::restart_as_STA(void* parameter) {
  // Cast parameter if needed (e.g., pass the server handle or other context)
  ESP_LOGI(TAG, "Restarting as STA mode...");

  ESP_ERROR_CHECK(esp_event_handler_instance_unregister(
      WIFI_EVENT, ESP_EVENT_ANY_ID, instance_any_id));
  ESP_LOGI(TAG, "Unregistered Wi-Fi event handlers.");

  vTaskDelay(pdMS_TO_TICKS(100));
  ESP_ERROR_CHECK(webserver::stop_webserver());
  vTaskDelay(pdMS_TO_TICKS(100));
  esp_wifi_stop();
  vTaskDelay(pdMS_TO_TICKS(100));
  esp_netif_destroy_default_wifi(wifi_esp_netif);
  vTaskDelay(pdMS_TO_TICKS(100));
  wifi_init_sta();
  vTaskDelete(NULL);  // Delete the task when done
}
}  // namespace network