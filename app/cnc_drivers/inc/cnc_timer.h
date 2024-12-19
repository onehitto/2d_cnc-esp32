#pragma once

#include <cstring>
#include <string>
#include "driver/gptimer.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

typedef struct {
  char name[32];      // store timer name
  int64_t timestamp;  // store the time when interrupt occurred
  int count;
} timer_queue_t;

class cnc_timer {
 public:
  cnc_timer(gptimer_alarm_cb_t callback, void* user_data);
  ~cnc_timer();
  /*static bool test_cb(gptimer_handle_t gptimer,
                      const gptimer_alarm_event_data_t* edata,
                      void* user_data);
*/
  void start_timer(int x_ms);
  void stop_timer();
  void set_alarm_value(int x_ms);

  gptimer_config_t gptimer_config;
  gptimer_handle_t gptimer;

  uint64_t alarm_value;
  int timer_counter;
  int64_t last;
  std::string name;

  static int number_of_timers;
  static QueueHandle_t timer_event_queue;
};