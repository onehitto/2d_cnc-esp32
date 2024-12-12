#include "cnc.h"

using namespace engine;
using namespace network;
using namespace nm_storage;

webserver ws;
storage storage;

#define TAG "CNC"
#define MAX_CONTENT_LENGTH 100

struct cnc_command_t {
  uint8_t command;
  char content[MAX_CONTENT_LENGTH];
  int sock_fd;
};

cnc::cnc()
    : gpio(),
      timer(cnc_callback, this),
      ws(),
      storage(),
      first_half(true),
      alarmchange(false) {
  memset(&block_to_exe, 0, sizeof(block_t));
  memset(&next_block, 0, sizeof(block_t));

  // set the system status to idle and no error
  memset(&status, 0, sizeof(status_t));

  // make sur the corrdinate are 0,0 float
  status.sys_coord.x = 0.0f;
  status.sys_coord.y = 0.0f;

  // default configuration
  config.step_resolution = FULL;
  config.mm_per_step_diag = 0.19f;
  config.mm_per_step_xy = config.mm_per_step_diag * sqrt(2);

  // defaut feedrate = 400 mm/min , cannot be zero
  set_feedrate(400);

  log.str("");
}

cnc::~cnc() {}

void cnc::cnc_init() {
  // init webserver(wifi nvs webserver)
  cnc_command_queue = xQueueCreate(10, sizeof(cnc_command_t));
  webserver::queue = cnc_command_queue;
  ESP_ERROR_CHECK(storage::init_spiffs());
  webserver::webserver_init();
  //   timer.start_timer(alarmvalue);
}

int cnc::cnc_config_cmd(char conf_n, std::string& line, int start) {
  // parse the configuration command
  // &1 1
  int end = line.find(' ', start);
  if (end == -1)
    end = line.length();

  std::string part = line.substr(start, end - start);

  if (conf_n == '1') {
    log << "Step resolution: ";
    if (part[0] == '1') {
      config.step_resolution = FULL;
      config.mm_per_step_diag = 0.19f;
      log << "Full step" << std::endl;
    } else if (part[0] == '2') {
      config.step_resolution = HALF;
      config.mm_per_step_diag = 0.10f;
      log << "Half step" << std::endl;
    } else if (part[0] == '3') {
      config.step_resolution = QUARTER;
      config.mm_per_step_diag = 0.05f;
      log << "Quarter step" << std::endl;
    } else if (part[0] == '4') {
      config.step_resolution = EIGHTH;
      config.mm_per_step_diag = 0.02f;
      log << "Eighth step" << std::endl;
    } else if (part[0] == '5') {
      config.step_resolution = SIXTEENTH;
      config.mm_per_step_diag = 0.016f;
      log << "Sixteenth step" << std::endl;
    }
    config.mm_per_step_xy = config.mm_per_step_diag * sqrt(2);
  } else if (conf_n == '2') {
    // set coordx, coordy to zero
    status.sys_coord.x = 0;
    status.sys_coord.y = 0;
  }

  return end + 1;
}

void cnc::cnc_parser(std::string& line) {
  int start = 0;

  while (start < line.length()) {
    int end = line.find(' ', start);
    if (end == -1)
      end = line.length();

    std::string part = line.substr(start, end - start);
    start = end + 1;

    if (part[0] == 'G') {
      switch (part[1]) {
        case '0':
          block_to_exe.motion = G0;
          break;
        case '1':
          block_to_exe.motion = G1;
          break;
        case '2':
          block_to_exe.motion = G2;
          break;
        case '3':
          block_to_exe.motion = G3;
          break;
        default:
          break;
      }
    } else if (part[0] == 'X') {
      block_to_exe.coord.x = std::stof(part.substr(1));
    } else if (part[0] == 'Y') {
      block_to_exe.coord.y = std::stof(part.substr(1));
    } else if (part[0] == 'F') {
      block_to_exe.feedrate = std::stoi(part.substr(1));
    } else if (part[0] == 'I') {
      block_to_exe.i = std::stof(part.substr(1));
    } else if (part[0] == 'J') {
      block_to_exe.j = std::stof(part.substr(1));
    } else if (part[0] == 'M') {
      switch (part[1]) {
        case '0':
          log << "M0: Program stop" << std::endl;
          break;
        default:
          break;
      }
    } else if (part[0] == '&') {
      start = cnc_config_cmd(part[1], line, start);
    } else {
      log << "Invalid command" << std::endl;
      status.sys_error = INVALID_COMMAND;
      break;
    }
  }
}

void cnc::cnc_cal_block() {
  int step_count = 0;
  if (block_to_exe.motion == G0 || block_to_exe.motion == G1) {
    // calculate the distance to move
    // Calculate the differences

    float dx = block_to_exe.coord.x - status.sys_coord.x;
    float dy = block_to_exe.coord.y - status.sys_coord.y;

    float xForward = dx > 0 ? 1.0f : -1.0f;
    float yForward = dy > 0 ? 1.0f : -1.0f;

    // calculate the time to move
    float temp_x = status.sys_coord.x;
    float temp_y = status.sys_coord.y;

    // save in block struct
    block_to_exe.linear.dx = dx;
    block_to_exe.linear.dy = dy;
    block_to_exe.linear.xForward = xForward;
    block_to_exe.linear.yForward = yForward;

    // Calculate the number of steps to move and calculate the time to
    //  execute block
    //  the line eqution that pass through the two points is Ax + By + C =0 :
    if (dx == 0) {
      //  - if the line is vertical x = x0 dont need to calculate the distance
      //  we just need to move in y direction.
      while (fabsf(temp_y - block_to_exe.coord.y) > config.mm_per_step_xy) {
        temp_y += yForward * config.mm_per_step_xy;
        step_count++;
      }
    } else if (dy == 0) {
      //  - if the line is horizontal y = y0 dont need to calculate the
      //  distance we just need to move in x direction.
      while (fabsf(temp_x - block_to_exe.coord.x) > config.mm_per_step_xy) {
        temp_x += xForward * config.mm_per_step_xy;
        step_count++;
      }

    } else if (dy == dx || dy == -dx) {
      // - if the line is diagonal slope of line m= (y1 - y0) / (x1 - x0) = 1
      // then we need to move in diagonal direction. else we check witch
      while (fabsf(temp_x - block_to_exe.coord.x) > config.mm_per_step_xy ||
             fabsf(temp_y - block_to_exe.coord.y) > config.mm_per_step_xy) {
        temp_x += xForward * config.mm_per_step_diag;
        temp_y += yForward * config.mm_per_step_diag;
        step_count++;
      }
    } else {
      //     A = - y1 + y0
      //     B = x1 - x0
      //     C = (y1 - y0) * x0 - (x1 -x0) * y0

      float A = -dy;
      float B = dx;
      float C = dy * status.sys_coord.x - dx * status.sys_coord.y;

      block_to_exe.linear.A = A;
      block_to_exe.linear.B = B;
      block_to_exe.linear.C = C;

      while (fabsf(temp_x - block_to_exe.coord.x) > config.mm_per_step_xy ||
             fabsf(temp_y - block_to_exe.coord.y) > config.mm_per_step_xy) {
        //     The distance between the point and the line is :
        float distance_Xinc = fabsf(
            A * (temp_x + xForward * config.mm_per_step_xy) + B * temp_y + C);
        float distance_Yinc = fabsf(
            A * temp_x + B * (temp_y + yForward * config.mm_per_step_xy) + C);
        ;
        float distance_Dinc =
            fabsf(A * (temp_x + xForward * config.mm_per_step_diag) +
                  B * (temp_y + yForward * config.mm_per_step_diag) + C);
        ;
        float min_distance =
            std::min({distance_Xinc, distance_Yinc, distance_Dinc});

        if (min_distance == distance_Xinc) {
          temp_x += xForward * config.mm_per_step_xy;
          step_count++;
        } else if (min_distance == distance_Yinc) {
          temp_y += yForward * config.mm_per_step_xy;
          step_count++;
        } else if (min_distance == distance_Dinc) {
          temp_x += xForward * config.mm_per_step_diag;
          temp_y += yForward * config.mm_per_step_diag;
          step_count++;
        }
      }
    }

  } else if (block_to_exe.motion == G2 || block_to_exe.motion == G3) {
    // calculate the centre of the circle

    float cx = block_to_exe.arc.cx = status.sys_coord.x + block_to_exe.i;
    float cy = block_to_exe.arc.cy = status.sys_coord.y + block_to_exe.j;
    float r = block_to_exe.arc.r =
        powf(block_to_exe.i, 2) + powf(block_to_exe.j, 2);

    float temp_x = status.sys_coord.x;
    float temp_y = status.sys_coord.y;

    float xForward(0.0);
    float yForward(0.0);

    while (fabsf(temp_x - block_to_exe.coord.x) > config.mm_per_step_xy ||
           fabsf(temp_y - block_to_exe.coord.y) > config.mm_per_step_xy) {
      // calculate the x and y direction
      if (temp_x - cx >= 0 && temp_y - cy >= 0) {
        xForward = block_to_exe.motion == G2 ? 1.0f : -1.0f;
        yForward = block_to_exe.motion == G2 ? -1.0f : 1.0f;
      } else if (temp_x - cx < 0 && temp_y - cy >= 0) {
        xForward = block_to_exe.motion == G2 ? 1.0f : -1.0f;
        yForward = block_to_exe.motion == G2 ? 1.0f : -1.0f;
      } else if (temp_x - cx <= 0 && temp_y - cy < 0) {
        xForward = block_to_exe.motion == G2 ? -1.0f : 1.0f;
        yForward = block_to_exe.motion == G2 ? 1.0f : -1.0f;
      } else if (temp_x - cx > 0 && temp_y - cy < 0) {
        xForward = block_to_exe.motion == G2 ? -1.0f : 1.0f;
        yForward = block_to_exe.motion == G2 ? -1.0f : 1.0f;
      }

      // calculate the distance to move
      float distance_xInc =
          fabsf(powf(temp_x + xForward * config.mm_per_step_xy - cx, 2) +
                powf(temp_y - cy, 2) - r);
      float distance_yInc =
          fabsf(powf(temp_x - cx, 2) +
                powf(temp_y + yForward * config.mm_per_step_xy - cy, 2) - r);
      float distance_dInc =
          fabsf(powf(temp_x + xForward * config.mm_per_step_diag - cx, 2) +
                powf(temp_y + yForward * config.mm_per_step_diag - cy, 2) - r);

      float min_distance =
          std::min({distance_xInc, distance_yInc, distance_dInc});

      if (min_distance == distance_xInc) {
        temp_x += xForward * config.mm_per_step_xy;
        step_count++;
      } else if (min_distance == distance_yInc) {
        temp_y += yForward * config.mm_per_step_xy;
        step_count++;
      } else if (min_distance == distance_dInc) {
        temp_x += xForward * config.mm_per_step_diag;
        temp_y += yForward * config.mm_per_step_diag;
        step_count++;
      }
    }
  }
  block_to_exe.step_count = step_count;
}

bool cnc::cnc_callback(gptimer_handle_t gptimer,
                       const gptimer_alarm_event_data_t* edata,
                       void* user_data) {
  cnc* self = static_cast<cnc*>(user_data);
  if (self == nullptr) {
    gptimer_stop(gptimer);
    return false;
  }

  if (self->alarmchange) {
    self->alarmchange = false;
    self->timer.set_alarm_value(self->config.alarmvalue);
  }

  if (self->first_half) {
    self->first_half = false;
    status_t& status = self->status;
    block_t& block_to_exe = self->block_to_exe;
    if (fabsf(status.sys_coord.x - block_to_exe.coord.x) >
            self->config.mm_per_step_xy ||
        fabsf(status.sys_coord.y - block_to_exe.coord.y) >
            self->config.mm_per_step_xy) {
      if (block_to_exe.motion == G0 || block_to_exe.motion == G1) {
        //  Calculate the number of steps to move and calculate the time to
        //  execute block
        //  the line eqution that pass through the two points is Ax + By + C =0
        //  :
        float dx = block_to_exe.linear.dx;
        float dy = block_to_exe.linear.dy;
        float xForward = block_to_exe.linear.xForward;
        float yForward = block_to_exe.linear.yForward;

        if (dx == 0) {
          //  - if the line is vertical x = x0 dont need to calculate the
          //  distance we just need to move in y direction.
          if (fabsf(status.sys_coord.y - block_to_exe.coord.y) >
              config.mm_per_step_xy) {
            status.sys_coord.y += yForward * config.mm_per_step_xy;
          }
        } else if (dy == 0) {
          //  - if the line is horizontal y = y0 dont need to calculate the
          //  distance we just need to move in x direction.
          if (fabsf(status.sys_coord.x - block_to_exe.coord.x) >
              config.mm_per_step_xy) {
            status.sys_coord.x += xForward * config.mm_per_step_xy;
          }

        } else if (dy == dx || dy == -dx) {
          // - if the line is diagonal slope of line m= (y1 - y0) / (x1 - x0) =
          // 1 then we need to move in diagonal direction. else we check witch
          if (fabsf(status.sys_coord.x - block_to_exe.coord.x) >
                  config.mm_per_step_xy ||
              fabsf(status.sys_coord.y - block_to_exe.coord.y) >
                  config.mm_per_step_xy) {
            status.sys_coord.x += xForward * config.mm_per_step_diag;
            status.sys_coord.y += yForward * config.mm_per_step_diag;
          }
        } else {
          //     A = - y1 + y0
          //     B = x1 - x0
          //     C = (y1 - y0) * x0 - (x1 -x0) * y0

          float A = block_to_exe.linear.A;
          float B = block_to_exe.linear.B;
          float C = block_to_exe.linear.C;

          if (fabsf(status.sys_coord.x - block_to_exe.coord.x) >
                  config.mm_per_step_xy ||
              fabsf(status.sys_coord.y - block_to_exe.coord.y) >
                  config.mm_per_step_xy) {
            //     The distance between the point and the line is :
            float distance_Xinc = fabsf(
                A * (status.sys_coord.x + xForward * config.mm_per_step_xy) +
                B * status.sys_coord.y + C);
            float distance_Yinc = fabsf(
                A * status.sys_coord.x +
                B * (status.sys_coord.y + yForward * config.mm_per_step_xy) +
                C);
            float distance_Dinc = fabsf(
                A * (status.sys_coord.x + xForward * config.mm_per_step_diag) +
                B * (status.sys_coord.y + yForward * config.mm_per_step_diag) +
                C);
            float min_distance =
                std::min({distance_Xinc, distance_Yinc, distance_Dinc});

            if (min_distance == distance_Xinc) {
              status.sys_coord.x += xForward * config.mm_per_step_xy;
            } else if (min_distance == distance_Yinc) {
              status.sys_coord.y += yForward * config.mm_per_step_xy;
            } else if (min_distance == distance_Dinc) {
              status.sys_coord.x += xForward * config.mm_per_step_diag;
              status.sys_coord.y += yForward * config.mm_per_step_diag;
            }
          }
        }
      } else if (block_to_exe.motion == G2 || block_to_exe.motion == G3) {
        // calculate the centre of the circle

        float cx = block_to_exe.arc.cx;
        float cy = block_to_exe.arc.cy;
        float r = block_to_exe.arc.r;

        float xForward(0.0);
        float yForward(0.0);

        if (fabsf(status.sys_coord.x - block_to_exe.coord.x) >
                config.mm_per_step_xy ||
            fabsf(status.sys_coord.y - block_to_exe.coord.y) >
                config.mm_per_step_xy) {
          // calculate the x and y direction
          if (status.sys_coord.x - cx >= 0 && status.sys_coord.y - cy >= 0) {
            xForward = block_to_exe.motion == G2 ? 1.0f : -1.0f;
            yForward = block_to_exe.motion == G2 ? -1.0f : 1.0f;
          } else if (status.sys_coord.x - cx < 0 &&
                     status.sys_coord.y - cy >= 0) {
            xForward = block_to_exe.motion == G2 ? 1.0f : -1.0f;
            yForward = block_to_exe.motion == G2 ? 1.0f : -1.0f;
          } else if (status.sys_coord.x - cx <= 0 &&
                     status.sys_coord.y - cy < 0) {
            xForward = block_to_exe.motion == G2 ? -1.0f : 1.0f;
            yForward = block_to_exe.motion == G2 ? 1.0f : -1.0f;
          } else if (status.sys_coord.x - cx > 0 &&
                     status.sys_coord.y - cy < 0) {
            xForward = block_to_exe.motion == G2 ? -1.0f : 1.0f;
            yForward = block_to_exe.motion == G2 ? -1.0f : 1.0f;
          }
          // calculate the distance to move
          float distance_xInc = fabsf(
              powf(status.sys_coord.x + xForward * config.mm_per_step_xy - cx,
                   2) +
              powf(status.sys_coord.y - cy, 2) - r);
          float distance_yInc = fabsf(
              powf(status.sys_coord.x - cx, 2) +
              powf(status.sys_coord.y + yForward * config.mm_per_step_xy - cy,
                   2) -
              r);
          float distance_dInc = fabsf(
              powf(status.sys_coord.x + xForward * config.mm_per_step_diag - cx,
                   2) +
              powf(status.sys_coord.y + yForward * config.mm_per_step_diag - cy,
                   2) -
              r);

          float min_distance =
              std::min({distance_xInc, distance_yInc, distance_dInc});

          if (min_distance == distance_xInc) {
            status.sys_coord.x += xForward * config.mm_per_step_xy;
          } else if (min_distance == distance_yInc) {
            status.sys_coord.y += yForward * config.mm_per_step_xy;
          } else if (min_distance == distance_dInc) {
            status.sys_coord.x += xForward * config.mm_per_step_diag;
            status.sys_coord.y += yForward * config.mm_per_step_diag;
          }
        }
      }
    } else {
      self->timer.stop_timer();
      self->status.sys_state = IDLE;
      self->first_half = true;
    }
  } else {
    self->first_half = true;
    self->gpio.setSTEP1(0);
    self->gpio.setSTEP2(0);
  }

  return false;
}

void cnc::cnc_task_entry(void* arg) {
  cnc* self = static_cast<cnc*>(arg);  // Cast the void* to cnc*
  if (self != nullptr) {
    self->cnc_task();  // Call the non-static member function
  }
}

void cnc::set_feedrate(int feedrate) {
  block_to_exe.feedrate = feedrate;
  config.alarmvalue = (int)(config.mm_per_step_diag * 500 * 60 / feedrate);
}

void cnc::cnc_task() {
  cnc_command_t cmd;
  timer_queue_t evt;
  char response[256];

  ESP_LOGI(TAG, "CNC Task started");

  while (true) {
    // Wait indefinitely for a command

    if (xQueueReceive(cnc_command_queue, &cmd, portMAX_DELAY)) {
      memset(response, 0, sizeof(response));
      ESP_LOGI(TAG, "Received command: %d", cmd.command);
      // Process the command
      if (cmd.command == GET_STATUS) {
        snprintf(response, sizeof(response),
                 "{\"response\":\"ok\",\"err_msg\":\"\",\"content\":{\"sys_state\":%d,\"line_number\":%d,
        ,\"x\":%.4f,\"y\":%.4f,\"resolution\":%d,\"speed\":%d,\"tick_timer\":%d,\"mm_per_step_"
                 "diag\":%.4f}}",
                 status.sys_state, storage.line_number, status.sys_coord.x,
                 status.sys_coord.y, config.step_resolution, block_to_exe.feedrate,config.alarmvalue,config.mm_per_step_diag);
      } else if (cmd.command == EXECUTE_GCODE && status.sys_state == IDLE) {
        std::string gcode_command = cmd.content;
        cnc_parser(gcode_command);
        cnc_cal_block();
        snprintf(response, sizeof(response),
                 "{\"response\":\"ok\",\"err_msg\":\"\",\"content\":{"
                 "\"numberofsteps\":%d}}",
                 block_exe.step_count);
        status.sys_state = RUNNING;
        timer.start_timer(config.alarmvalue);
      } else if (cmd.command == CONFIG && status.sys_state == IDLE) {
        // todo
        std::string config = cmd.content;
        snprintf(response, 128, "{\"type\":\"config\",\"file\":\"%s\"}",
                 config.c_str());
      } else if (cmd.command == RUN_FILE && status.sys_state == IDLE) {
        ESP_LOGI(TAG, "-->RUN_FILE ");
        storage::open_file("exe_g.txt", "r");
        storage::close_file();
        ESP_LOGI(TAG, "File upload complete total_line : %d",
                 storage::total_lines);
      } else if (cmd.command == PAUSE && status.sys_state == RUNNING) {
        ESP_LOGI(TAG, "-->PAUSE ");
      } else if (cmd.command == RESUME && status.sys_state == PAUSED) {
        ESP_LOGI(TAG, "-->RESUME ");
      } else if (cmd.command == STOP &&
                 (status.sys_state == PAUSED || status.sys_state == RUNNING)) {
        ESP_LOGI(TAG, "-->STOP ");
      } else {
        snprintf(response, sizeof(response),
                 "{\"status\":\"error\",\"message\":\"Unknown command\"}");
      }
      if (strlen(response) > 0) {
        esp_err_t ret = webserver::ws_send(webserver::server, cmd.sock_fd,
                                           response, strlen(response));
        if (ret != ESP_OK) {
          ESP_LOGE(TAG, "Failed to send WebSocket message: %s",
                   esp_err_to_name(ret));
        }
      }
      // Add additional command handling as needed
    }
  }
}
