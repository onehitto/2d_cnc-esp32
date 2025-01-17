#include "webserver.h"

namespace network {
using namespace nm_storage;
#define TAG "WEB_SERVER"
// TODO: should studythe max payload length with speed
#define MAX_WS_PAYLOAD_LEN 2500  // Set a maximum length (1 KB in this example)
#define MAX_CONTENT_LENGTH 100

#define RESP_MSG(buffer, buffer_size, resp, err, c)                         \
  snprintf(buffer, buffer_size,                                             \
           "{\"response\":\"%s\",\"err_msg\":\"%s\",\"content\":\"{%s}\"}", \
           resp, err, c)

struct cnc_command_t {
  uint8_t command;
  char content[MAX_CONTENT_LENGTH];
  int sock_fd;
};

// global variables
wifi_config_t webserver::wifi_config = {};
httpd_handle_t webserver::server = NULL;
esp_netif_t* webserver::wifi_esp_netif = NULL;

static esp_event_handler_instance_t instance_any_id;
static esp_event_handler_instance_t instance_got_ip;
static file_upload_state_t file_upload_state;

int webserver::count_try = 0;

char webserver::ssid_buffer[32] = {0};  // Initialize with null characters
char webserver::password_buffer[64] = {0};
bool webserver::credentials_valid = false;

QueueHandle_t webserver::queue;
EventGroupHandle_t webserver::event_group;

// WebSocket frames
httpd_ws_frame_t msg_frame = {.final = true,
                              .fragmented = false,
                              .type = HTTPD_WS_TYPE_TEXT,
                              .payload = nullptr,
                              .len = 0};

// websocket uri handlers
static httpd_uri_t https_set_creadentials = {
    .uri = "/set-creadentials",
    .method = HTTP_POST,
    .handler = webserver::handle_https_set_credentials,
    .user_ctx = NULL,
    .is_websocket = false,
    .handle_ws_control_frames = false,
    .supported_subprotocol = NULL};

static httpd_uri_t ws_cnc = {.uri = "/ws",
                             .method = HTTP_GET,
                             .handler = webserver::handle_ws_cnc,
                             .user_ctx = NULL,
                             .is_websocket = true,
                             .handle_ws_control_frames = false,
                             .supported_subprotocol = NULL};

webserver::webserver() {}

void webserver::webserver_init() {
  esp_netif_init();
  esp_event_loop_create_default();
  esp_wifi_set_ps(WIFI_PS_NONE);

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
  strncpy((char*)wifi_config.ap.ssid, "2d_cnc_esp32", 12);
  wifi_config.ap.ssid_len = strlen("2d_cnc_esp32");
  wifi_config.ap.max_connection = 4;
  wifi_config.ap.authmode = WIFI_AUTH_WPA_WPA2_PSK;
  strncpy((char*)wifi_config.ap.password, "12345678", 8);
  wifi_config.ap.ssid_hidden = 0;

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
      httpd_register_uri_handler(server, &https_set_creadentials);
    else {
      httpd_register_uri_handler(server, &ws_cnc);
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

esp_err_t webserver::handle_https_set_credentials(httpd_req_t* req) {
  if (req->method != HTTP_POST) {
    ESP_LOGE(TAG, "Unsupported method. Only POST is allowed.");
    httpd_resp_send_err(req, HTTPD_405_METHOD_NOT_ALLOWED,
                        "Method not allowed");
    return ESP_ERR_INVALID_ARG;
  }

  char buffer[150];                  // Buffer for receiving request data
  int total_len = req->content_len;  // Length of the incoming data
  int received = 0;

  if (total_len >= sizeof(buffer)) {
    ESP_LOGE(TAG, "Payload too large.");
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Payload too large");
    return ESP_ERR_INVALID_SIZE;
  }

  // Receive the request body
  while (received < total_len) {
    int ret = httpd_req_recv(req, buffer + received, total_len - received);
    if (ret <= 0) {
      ESP_LOGE(TAG, "Error receiving request body: %d", ret);
      if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
        httpd_resp_send_err(req, HTTPD_408_REQ_TIMEOUT, "Request timeout");
      } else {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
                            "Internal server error");
      }
      return ESP_FAIL;
    }
    received += ret;
  }
  buffer[received] = '\0';  // Null-terminate the received data

  ESP_LOGD(TAG, "Received credentials payload: %s", buffer);

  // Process the received credentials
  if (!process_creadentials_msg(buffer)) {
    ESP_LOGE(TAG, "Invalid credentials received.");
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid credentials");
    return ESP_ERR_INVALID_ARG;
  }

  // If credentials are valid, save them
  save_credentials();

  // Send a response to acknowledge the credentials
  const char resp[] = "Credentials updated successfully";
  httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);

  if (xTaskCreate(tsk_restart_as_STA, "restart_as_STA", 4096, NULL, 5, NULL) !=
      pdPASS) {
    ESP_LOGE(TAG, "Failed to create STA restart task.");
    return ESP_ERR_NO_MEM;
  }

  return ESP_OK;
}

esp_err_t webserver::handle_ws_cnc(httpd_req_t* req) {
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
  if (ws_pkt.len > MAX_WS_PAYLOAD_LEN) {
    ESP_LOGE(TAG, "WebSocket payload too large: %d bytes (max: %d)", ws_pkt.len,
             MAX_WS_PAYLOAD_LEN);
    const char* err_msg =
        "{\"response\":\"error\",\"error\":\"Payload > 1kb\",\"content\":\"\"}";
    msg_frame.payload = (uint8_t*)err_msg;
    msg_frame.len = strlen(err_msg),

    httpd_ws_send_frame(req, &msg_frame);

    return ESP_ERR_INVALID_SIZE;  // Return an error code
  }

  if (ws_pkt.len > 0) {
    ws_pkt.payload =
        (uint8_t*)malloc(ws_pkt.len + 1);  // +1 for null termination
    if (!ws_pkt.payload) {
      ESP_LOGE(TAG, "Failed to allocate memory for WebSocket payload");
      return ESP_ERR_NO_MEM;
    }
  }

  // Receive the payload
  ret = httpd_ws_recv_frame(req, &ws_pkt, ws_pkt.len);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "httpd_ws_recv_frame failed: %d", ret);
    free(ws_pkt.payload);
    return ret;
  }
  ws_pkt.payload[ws_pkt.len] = '\0';  // Null-terminate for safety

  // Process the WebSocket message
  process_ws_message(req, (const char*)ws_pkt.payload);
  // Free the payload memory
  if (ws_pkt.payload) {
    free(ws_pkt.payload);
  }

  return ESP_OK;
}

esp_err_t webserver::process_ws_message(httpd_req_t* req, const char* message) {
  cJSON* json = cJSON_Parse(message);
  if (!json) {
    ESP_LOGE(TAG, "Failed to parse JSON");
    return ESP_FAIL;
  }
  char msg[256];

  cnc_command_t cmd;
  memset(&cmd, 0, sizeof(cnc_command_t));  // Initialize the command struct
  cmd.sock_fd = httpd_req_to_sockfd(req);  // Get the socket file descriptor

  const char* type = cJSON_GetObjectItem(json, "type")->valuestring;
  ESP_LOGI(TAG, "------>Received message type: %s", type);

  if (strcmp(type, "get_status") == 0) {
    cmd.command = GET_STATUS;
  } else if (strcmp(type, "exe_gcode") == 0) {
    cmd.command = EXECUTE_GCODE;
    const char* content = cJSON_GetObjectItem(json, "data")->valuestring;
    strncpy(cmd.content, content, MAX_CONTENT_LENGTH - 1);
    cmd.content[MAX_CONTENT_LENGTH - 1] = '\0';  // Ensure null-termination
  } else if (strcmp(type, "config") == 0) {
    cmd.command = CONFIG;
    const char* content = cJSON_GetObjectItem(json, "data")->valuestring;
    strncpy(cmd.content, content, MAX_CONTENT_LENGTH - 1);
    cmd.content[MAX_CONTENT_LENGTH - 1] = '\0';  // Ensure null-termination
  } else if (strcmp(type, "run_file") == 0) {
    cmd.command = RUN_FILE;
  } else if (strcmp(type, "pause") == 0) {
    cmd.command = PAUSE;
  } else if (strcmp(type, "resume") == 0) {
    cmd.command = RESUME;
  } else if (strcmp(type, "stop") == 0) {
    cmd.command = STOP;
  } else if (strcmp(type, "upload_file") == 0) {
    int chunk = (int)cJSON_GetObjectItem(json, "chunk_number")->valuedouble;
    int total_chunks =
        (int)cJSON_GetObjectItem(json, "total_chunks")->valuedouble;
    const char* data = cJSON_GetObjectItem(json, "data")->valuestring;

    ESP_LOGI(TAG, "Received upload_file chunk %d of %d", chunk, total_chunks);

    if (chunk == 1) {
      if (ESP_OK != storage::open_file("exe_g.txt", "w")) {
        RESP_MSG(msg, sizeof(msg), "error", "open file exe_g failed", "");
        ws_send(req->handle, httpd_req_to_sockfd(req), msg, strlen(msg));
        cJSON_Delete(json);
        return ESP_FAIL;
      }
      if (ESP_OK != storage::write_file(data)) {
        RESP_MSG(msg, sizeof(msg), "error", "write file exe_g failed", "");
        ws_send(req->handle, httpd_req_to_sockfd(req), msg, strlen(msg));
        cJSON_Delete(json);
        return ESP_FAIL;
      }
      file_upload_state.current_chunk = chunk;
      file_upload_state.total_chunks = total_chunks;
      RESP_MSG(msg, sizeof(msg), "success", "Chunk received", "");
      ws_send(req->handle, httpd_req_to_sockfd(req), msg, strlen(msg));
    } else {
      if (chunk != file_upload_state.current_chunk + 1) {
        ESP_LOGE(TAG, "Received out-of-order chunk %d (expected %d)", chunk,
                 file_upload_state.current_chunk + 1);
        RESP_MSG(msg, sizeof(msg), "error", "Out-of-order chunk", "");
        ws_send(req->handle, httpd_req_to_sockfd(req), msg, strlen(msg));
        cJSON_Delete(json);
        return ESP_FAIL;
      }
      if (ESP_OK != storage::write_file(data)) {
        RESP_MSG(msg, sizeof(msg), "error", "write file exe_g failed", "");
        ws_send(req->handle, httpd_req_to_sockfd(req), msg, strlen(msg));
        cJSON_Delete(json);
        return ESP_FAIL;
      }
      file_upload_state.current_chunk++;
      if (file_upload_state.current_chunk == total_chunks) {
        storage::close_file();
        ESP_LOGI(TAG, "File upload complete");
        RESP_MSG(msg, sizeof(msg), "success", "File upload complete", "");
        ws_send(req->handle, httpd_req_to_sockfd(req), msg, strlen(msg));
      } else {
        RESP_MSG(msg, sizeof(msg), "success", "Chunk received", "");
        ws_send(req->handle, httpd_req_to_sockfd(req), msg, strlen(msg));
      }
    }
    cJSON_Delete(json);
    return ESP_OK;
  } else {
    ESP_LOGW(TAG, "Unknown message type: %s", type);
    cJSON_Delete(json);
    return ESP_FAIL;
  }

  // Send the command to the CNC task via the queue
  if (xQueueSend(queue, &cmd, 0) != pdPASS) {
    char msg[256];
    RESP_MSG(msg, sizeof(msg), "error", "Failed to send command to CNC task",
             "");
    ws_send(req->handle, httpd_req_to_sockfd(req), msg, strlen(msg));
  }

  // send event to the CNC task
  xEventGroupSetBits(event_group, EVENT_CMD);

  cJSON_Delete(json);
  return ESP_OK;
}

bool webserver::process_creadentials_msg(const char* buffer) {
  bool ssid_received = false;
  bool password_received = false;
  cJSON* root = cJSON_Parse(buffer);
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
  return credentials_valid;
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
        if (xTaskCreate(tsk_restart_as_AP, "tsk_restart_as_AP", 4096, NULL, 5,
                        NULL) != pdPASS) {
          ESP_LOGE(TAG, "Failed to create STA restart task.");
        }
      }
    } else if (event_id == WIFI_EVENT_STA_CONNECTED) {
      wifi_event_sta_connected_t* event =
          (wifi_event_sta_connected_t*)event_data;
      ESP_LOGI(TAG, "Connected to Wi-Fi. SSID: %s", event->ssid);
    } else if (event_id == WIFI_EVENT_AP_STOP) {
      ESP_LOGW(TAG, "AP has stopped unexpectedly!");
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

void webserver::tsk_restart_as_STA(void* parameter) {
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

void webserver::tsk_restart_as_AP(void* parameter) {
  ESP_ERROR_CHECK(webserver::stop_webserver());
  ESP_LOGI(TAG, "Unregistered Wi-Fi event handlers.");
  ESP_ERROR_CHECK(esp_event_handler_instance_unregister(
      WIFI_EVENT, ESP_EVENT_ANY_ID, instance_any_id));
  ESP_ERROR_CHECK(esp_event_handler_instance_unregister(
      WIFI_EVENT, IP_EVENT_STA_GOT_IP, instance_got_ip));

  ESP_LOGI(TAG, "Stopping Wi-Fi...");
  esp_wifi_stop();
  ESP_LOGI(TAG, "Destroying default Wi-Fi interface...");
  esp_netif_destroy_default_wifi(wifi_esp_netif);

  wifi_init_ap();  // Switch to AP mode

  vTaskDelete(NULL);
}

esp_err_t webserver::ws_send(httpd_handle_t hd,
                             int fd,
                             const char* data,
                             size_t len) {
  httpd_ws_frame_t ws_pkt;
  memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
  ws_pkt.payload = (uint8_t*)data;
  ws_pkt.len = len;
  ws_pkt.type = HTTPD_WS_TYPE_TEXT;
  ws_pkt.final = true;
  ws_pkt.fragmented = false;
  return httpd_ws_send_frame_async(hd, fd, &ws_pkt);
}

}  // namespace network