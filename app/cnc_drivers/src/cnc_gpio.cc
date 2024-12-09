#include "cnc_gpio.h"

cnc_gpio::cnc_gpio() {
  // Configure output pins
  configureOutput(ENB);
  configureOutput(MS1);
  configureOutput(MS2);
  configureOutput(MS3);

  configureOutput(STEP1);
  configureOutput(DIR1);

  configureOutput(STEP2);
  configureOutput(DIR2);

  // Configure input pins
  configureInput(SENSOR1);
  configureInput(SENSOR2);
}

void cnc_gpio::configureOutput(gpio_num_t pin) {
  gpio_config_t io_conf = {};
  io_conf.intr_type = GPIO_INTR_DISABLE;
  io_conf.mode = GPIO_MODE_OUTPUT;
  io_conf.pin_bit_mask = (1ULL << pin);
  io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
  io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
  gpio_config(&io_conf);

  // Optional: Set default low output
  gpio_set_level(pin, 0);
}

void cnc_gpio::configureInput(gpio_num_t pin) {
  gpio_config_t io_conf = {};
  io_conf.intr_type = GPIO_INTR_DISABLE;
  io_conf.mode = GPIO_MODE_INPUT;
  io_conf.pin_bit_mask = (1ULL << pin);
  io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
  io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
  gpio_config(&io_conf);
}