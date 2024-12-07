#pragma once

#include "driver/timer.h"
#include "esp_clk.h"
#include "esp_err.h"
#include "esp_log.h"

#define CNC_GROUP_TIMER TIMER_GROUP_1
#define CNC_TIMER TIMER_0

#define MHZ_1 1000000
#define CALC_ALARM_VALUE(x_ms) ((MHZ_1) * (x_ms) / 1000)

class cnc_timer {
 public:
  cnc_timer();

  static void timer_isr(void* arg);
  void start_timer(int x_ms);
  void stop_timer();
  void set_alarm_value(int x_ms);

  static timer_config_t timer_config;
  static uint64_t alarm_value;
};