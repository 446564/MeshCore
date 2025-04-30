#pragma once

#include <Arduino.h>
#include "ESP32Board.h"
#include <driver/rtc_io.h>
#define XPOWERS_CHIP_BQ25896
#include "XPowersLib.h"

#define P_LORA_DIO_1 5
#define P_LORA_NSS 3
#define P_LORA_RESET 4
#define P_LORA_BUSY 6
#define P_LORA_SCLK 36
#define P_LORA_MISO 47
#define P_LORA_MOSI 33

#define BOARD_LORA_EN 46

#define BOARD_I2C_ADDR_BQ25896 0x6B

class TDeckProBoard : public ESP32Board
{
private:
  PowersBQ25896 PPM;

public:
  void begin()
  {

    ESP32Board::begin();

    // taken from factory firmware example
    Wire.beginTransmission(BOARD_I2C_ADDR_BQ25896);
    if (Wire.endTransmission() == 0)
    {
      PPM.init(Wire, PIN_BOARD_SDA, PIN_BOARD_SCL, BOARD_I2C_ADDR_BQ25896);
      PPM.setSysPowerDownVoltage(3300);
      PPM.setInputCurrentLimit(3250);
      PPM.disableCurrentLimitPin();
      PPM.setChargeTargetVoltage(4208);
      PPM.setPrechargeCurr(64);
      PPM.setChargerConstantCurr(832);
      PPM.enableMeasure(); // was enableADCMeasure which doesn't exist?
      PPM.enableCharge();

      // The OTG function needs to enable OTG, and set the OTG control pin to HIGH
      // After OTG is enabled, if an external power supply is plugged in, OTG will be turned off
      PPM.enableOTG();
      PPM.disableOTG();
    }

    esp_reset_reason_t reason = esp_reset_reason();

    if (reason == ESP_RST_DEEPSLEEP)
    {
      long wakeup_source = esp_sleep_get_ext1_wakeup_status();
      if (wakeup_source & (1 << P_LORA_DIO_1))
      { // received a LoRa packet (while in deep sleep)
        startup_reason = BD_STARTUP_RX_PACKET;
      }

      rtc_gpio_hold_dis((gpio_num_t)P_LORA_NSS);
      rtc_gpio_deinit((gpio_num_t)P_LORA_DIO_1);
    }

    // LORA、SD、EPD use the same SPI, in order to avoid mutual influence;
    // // before powering on, all CS signals should be pulled high and in an unselected state;
    pinMode(P_LORA_NSS, OUTPUT);
    digitalWrite(P_LORA_NSS, HIGH);

    Serial.begin(115200);

    // enable lora module
    pinMode(BOARD_LORA_EN, OUTPUT);
    digitalWrite(BOARD_LORA_EN, HIGH);

    // SPI
    SPI.begin(P_LORA_SCLK, P_LORA_MISO, P_LORA_MOSI);

    // TODO: fuel gauge -- bq27220.init();
  }

  void enterDeepSleep(uint32_t secs, int pin_wake_btn = -1)
  {
    esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON);

    // Make sure the DIO1 and NSS GPIOs are hold on required levels during deep sleep
    rtc_gpio_set_direction((gpio_num_t)P_LORA_DIO_1, RTC_GPIO_MODE_INPUT_ONLY);
    rtc_gpio_pulldown_en((gpio_num_t)P_LORA_DIO_1);

    rtc_gpio_hold_en((gpio_num_t)P_LORA_NSS);

    if (pin_wake_btn < 0)
    {
      esp_sleep_enable_ext1_wakeup((1L << P_LORA_DIO_1), ESP_EXT1_WAKEUP_ANY_HIGH); // wake up on: recv LoRa packet
    }
    else
    {
      esp_sleep_enable_ext1_wakeup((1L << P_LORA_DIO_1) | (1L << pin_wake_btn), ESP_EXT1_WAKEUP_ANY_HIGH); // wake up on: recv LoRa packet OR wake btn
    }

    if (secs > 0)
    {
      esp_sleep_enable_timer_wakeup(secs * 1000000);
    }
  }

  uint16_t getBattMilliVolts() override
  {

    return PPM.getBattVoltage();
  }

  const char *getManufacturerName() const override
  {
    return "LILYGO T-Deck Pro";
  }
};
