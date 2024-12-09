#pragma once

#include <algorithm>
#include <cmath>
#include <cstring>
#include <iostream>
#include <ostream>
#include <sstream>
#include "cnc_timer.h"
#include "webserver.h"

namespace engine {

enum Motion { G0, G1, G2, G3 };
enum State { IDLE, RUNNING, PAUSED, HOMING, STOPPED };
enum Error { NO_ERROR, INVALID_COMMAND };
enum Step_resolution { FULL, HALF, QUARTER, EIGHTH, SIXTEENTH };

typedef struct {
  float x;
  float y;
} coord_t;

typedef struct {
  Motion motion;
  coord_t coord;
  float i, j;
  int feedrate;
  struct {
    float dx, dy;
    float m;
    float A, B, C;
    float xForward, yForward;

  } linear;
  struct {
    float cx, cy;
    float r;
  } arc;
  int step_count;
} block_t;

typedef struct {
  Step_resolution step_resolution;
  float mm_per_step_xy;
  float mm_per_step_diag;
} config_t;

typedef struct {
  State sys_state;
  Error sys_error;
  coord_t sys_coord;
} status_t;

class cnc {
 public:
  cnc();
  ~cnc();

  // init the cnc + webserver + storage + timer + queue
  void cnc_init();

  // processing
  void cnc_parser(std::string& line);
  void cnc_cal_block();
  int cnc_config_cmd(char conf_n, std::string& line, int start);

  // execute
  static bool cnc_callback(gptimer_handle_t gptimer,
                           const gptimer_alarm_event_data_t* edata,
                           void* user_data);

  // tasks
  void cnc_task();
  static void cnc_task_entry(void* arg);

  std::stringstream log;

  QueueHandle_t cnc_command_queue;
  block_t block_to_exe;
  block_t next_block;
  config_t config;
  status_t status;

  cnc_timer timer;
  int count;
  static network::webserver ws;
  static nm_storage::storage storage;

 private:
  bool first_half;
};
}  // namespace engine
