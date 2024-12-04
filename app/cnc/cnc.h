#pragma once

#include <algorithm>
#include <cmath>
#include <cstring>
#include <iostream>
#include <ostream>
#include <sstream>
#include "webserver.h"

namespace engine {

enum Motion { G0, G1, G2, G3 };
enum Status { IDLE, RUNNING, PAUSED, ERROR };
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

class cnc {
 public:
  cnc();
  ~cnc();

  void cnc_init();
  // init gpio IO + PWM + timer to drive Motors + LED
  // void cnc_hardware_init();
  // // read and write a file
  // void cnc_file_sys_init();

  void cnc_parser(std::string& line);
  void cnc_cal_block();

  int cnc_config_cmd(char conf_n, std::string& line, int start);
  void cnc_exe_handler();

  QueueHandle_t cnc_command_queue;

  void cnc_task();

  static void cnc_task_entry(void* arg);

  block_t block_to_exe;
  block_t next_block;

  struct {
    Step_resolution step_resolution;
    float mm_per_step_xy;
    float mm_per_step_diag;
  } config;

  struct {
    Status sys_status;
    Error sys_error;
    coord_t sys_coord;
  } status;

  std::ostringstream log;

  static network::webserver ws;
  static nm_storage::storage storage;
};
}  // namespace engine
