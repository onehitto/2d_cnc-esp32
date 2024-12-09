#pragma once

#include "driver/gpio.h"
#include "esp_err.h"

// A wrapper class to handle initialization and control of the specified GPIO
// pins
class cnc_gpio {
 public:
  // Define pin constants for clarity
  static constexpr gpio_num_t ENB = GPIO_NUM_39;
  static constexpr gpio_num_t MS1 = GPIO_NUM_40;
  static constexpr gpio_num_t MS2 = GPIO_NUM_41;
  static constexpr gpio_num_t MS3 = GPIO_NUM_42;

  static constexpr gpio_num_t STEP1 = GPIO_NUM_37;
  static constexpr gpio_num_t DIR1 = GPIO_NUM_38;

  static constexpr gpio_num_t STEP2 = GPIO_NUM_35;
  static constexpr gpio_num_t DIR2 = GPIO_NUM_36;

  static constexpr gpio_num_t SENSOR1 = GPIO_NUM_15;
  static constexpr gpio_num_t SENSOR2 = GPIO_NUM_16;

  cnc_gpio();

  // Set output pin to HIGH or LOW
  esp_err_t setEnb(bool level) { return gpio_set_level(ENB, level ? 1 : 0); }
  esp_err_t setMS1(bool level) { return gpio_set_level(MS1, level ? 1 : 0); }
  esp_err_t setMS2(bool level) { return gpio_set_level(MS2, level ? 1 : 0); }
  esp_err_t setMS3(bool level) { return gpio_set_level(MS3, level ? 1 : 0); }

  esp_err_t setSTEP1(bool level) {
    return gpio_set_level(STEP1, level ? 1 : 0);
  }
  esp_err_t setDIR1(bool level) { return gpio_set_level(DIR1, level ? 1 : 0); }

  esp_err_t setSTEP2(bool level) {
    return gpio_set_level(STEP2, level ? 1 : 0);
  }
  esp_err_t setDIR2(bool level) { return gpio_set_level(DIR2, level ? 1 : 0); }

  // Read input from sensor pins
  int readSensor1() { return gpio_get_level(SENSOR1); }
  int readSensor2() { return gpio_get_level(SENSOR2); }

 private:
  void configureOutput(gpio_num_t pin);
  void configureInput(gpio_num_t pin);
};