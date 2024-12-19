#include "cnc_timer.h"

#define TAG "CNC_TIMER"

int cnc_timer::number_of_timers = 0;

QueueHandle_t cnc_timer::timer_event_queue = nullptr;

cnc_timer::cnc_timer(gptimer_alarm_cb_t callback, void* user_data)
    : gptimer(NULL), alarm_value(1), timer_counter(0), last(0) {
  // set the name of the timer
  name = "Timer " + std::to_string(number_of_timers);

  if (timer_event_queue == nullptr) {
    // Create the queue to send the timer events
    timer_event_queue = xQueueCreate(10, sizeof(timer_queue_t));
  }

  // Set the timer configuration
  gptimer_config.direction = GPTIMER_COUNT_UP;
  gptimer_config.clk_src = GPTIMER_CLK_SRC_APB;
  gptimer_config.resolution_hz = 1 * 1000 * 1000;  // 1MHz = 1us
  gptimer_config.intr_priority = 0;
  gptimer_config.flags.allow_pd = false;
  gptimer_config.flags.backup_before_sleep = false;
  // Create the timer
  ESP_ERROR_CHECK(gptimer_new_timer(&gptimer_config, &gptimer));
  // Register the timer event callback
  gptimer_event_callbacks_t cbs = {
      .on_alarm = callback,
  };
  ESP_ERROR_CHECK(gptimer_register_event_callbacks(gptimer, &cbs, user_data));

  // Enable the timer (does not start counting yet)
  ESP_ERROR_CHECK(gptimer_enable(gptimer));

  // Increment the number of timers
  number_of_timers++;
}
/*
bool cnc_timer::test_cb(gptimer_handle_t gptimer,
                        const gptimer_alarm_event_data_t* edata,
                        void* user_data) {
  cnc_timer* tim = (cnc_timer*)user_data;
  if (tim->last == 0)
    tim->last = esp_timer_get_time();
  int64_t now = esp_timer_get_time();
  timer_queue_t evt;
  strncpy(evt.name, tim->name.c_str(), sizeof(evt.name) - 1);
  evt.name[sizeof(evt.name) - 1] = '\0';  // Ensure null termination
  evt.timestamp = now - tim->last;
  evt.count = tim->timer_counter;
  tim->last = now;

  if (tim->timer_counter == 15) {
    tim->stop_timer();
  } else if (tim->timer_counter == 15) {
    tim->set_alarm_value(10000);
  } else if (tim->timer_counter == 10) {
    tim->set_alarm_value(5000);
  } else if (tim->timer_counter == 5) {
    tim->set_alarm_value(3000);
  }
  tim->timer_counter++;

  // Send event data to the queue from ISR
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  xQueueSendFromISR(timer_event_queue, &evt, &xHigherPriorityTaskWoken);

  // If sending to the queue caused a task to unblock and a higher priority
  // task should run now, request a context switch.
  if (xHigherPriorityTaskWoken == pdTRUE) {
    portYIELD_FROM_ISR();
  }

  return false;
}*/

void cnc_timer::start_timer(int x_ms) {
  // Reset the timer counter
  timer_counter = 0;
  ESP_ERROR_CHECK(gptimer_set_raw_count(gptimer, 0));

  // Configure the alarm value
  gptimer_alarm_config_t alarm_config = {};
  alarm_config.alarm_count = x_ms;
  alarm_config.flags.auto_reload_on_alarm = true;
  alarm_config.reload_count = 0;
  ESP_ERROR_CHECK(gptimer_set_alarm_action(gptimer, &alarm_config));

  // Start the timer
  ESP_ERROR_CHECK(gptimer_start(gptimer));
}

void cnc_timer::set_alarm_value(int x_ms) {
  // Set the alarm value for the desired period
  gptimer_alarm_config_t alarm_config = {};
  alarm_config.alarm_count = x_ms;
  alarm_config.flags.auto_reload_on_alarm = true;
  alarm_config.reload_count = 0;

  ESP_ERROR_CHECK(gptimer_set_alarm_action(gptimer, &alarm_config));
}

void cnc_timer::stop_timer() {
  // Stop the timer
  ESP_ERROR_CHECK(gptimer_stop(gptimer));
}

cnc_timer::~cnc_timer() {
  // Delete the timer
  stop_timer();
  gptimer_disable(gptimer);
  ESP_ERROR_CHECK(gptimer_del_timer(gptimer));
}