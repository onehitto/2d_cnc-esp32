#include "cnc_timer.h"

#define TAG "CNC_TIMER"

cnc_timer::cnc_timer() {
  uint32_t cpu_freq_hz = esp_clk_cpu_freq();
  uint32_t divider = cpu_freq_hz / MHZ_1;

  timer_config.alarm_en = TIMER_ALARM_EN;
  timer_config.counter_en = TIMER_PAUSE;
  timer_config.intr_type = TIMER_INTR_LEVEL;
  timer_config.counter_dir = TIMER_COUNT_UP;
  timer_config.auto_reload = TIMER_AUTORELOAD_EN;
  timer_config.divider = divider;

  // Initialize the timer
  timer_init(CNC_GROUP_TIMER, CNC_TIMER, &timer_config);

  // Set the counter value to 0
  timer_set_counter_value(CNC_GROUP_TIMER, CNC_TIMER, 0);

  // Enable the timer interrupt
  timer_enable_intr(CNC_GROUP_TIMER, CNC_TIMER);

  timer_isr_register(CNC_GROUP_TIMER, CNC_TIMER, timer_isr, NULL,
                     ESP_INTR_FLAG_IRAM, NULL);
}

void cnc_timer::timer_isr(void* arg) {
  // Clear the interrupt status
  static uint32_t timer_counter = 0;
  timer_group_clr_intr_status_in_isr(CNC_GROUP_TIMER, CNC_TIMER);
  ESP_LOGI(TAG, "Timer interrupt");
  timer_counter++;
  if (timer_counter > 10)
    stop_timer();
}

void cnc_timer::start_timer(int x_ms) {
  // Set the counter value to 0
  set_alarm_value(x_ms);
  timer_set_counter_value(CNC_GROUP_TIMER, CNC_TIMER, 0);

  // Start the timer
  timer_start(CNC_GROUP_TIMER, CNC_TIMER);
}

void cnc_timer::stop_timer() {
  // Stop the timer
  timer_pause(CNC_GROUP_TIMER, CNC_TIMER);
}

void cnc_timer::set_alarm_value(int x_ms) {
  // Set the alarm value for the desired period
  timer_set_alarm_value(CNC_GROUP_TIMER, CNC_TIMER, CALC_ALARM_VALUE(x_ms));
}