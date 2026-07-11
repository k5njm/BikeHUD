#include "power.h"

#include "board_pins.h"
#include "bike_hud_protocol.h"

#include <Fonts/FreeSansBold12pt7b.h>
#include <Fonts/FreeSansBold18pt7b.h>
#include <Fonts/FreeSansBold24pt7b.h>
#include <GxEPD2_BW.h>
#include <NimBLEDevice.h>
#include <SPI.h>
#include <esp_sleep.h>

// Same panel instance pattern as hud.cpp — separate object is fine for a
// one-shot splash before sleep (SPI already configured).
static GxEPD2_BW<GxEPD2_426_GDEQ0426T82, GxEPD2_426_GDEQ0426T82::HEIGHT>
    sleep_display(GxEPD2_426_GDEQ0426T82(/*CS=*/PIN_EPD_CS, /*DC=*/PIN_EPD_DC,
                                        /*RST=*/PIN_EPD_RST,
                                        /*BUSY=*/PIN_EPD_BUSY));

namespace {

void draw_sleep_splash() {
  // Panel may already be inited by hud; re-init is safe enough for shutdown path.
  pinMode(PIN_EPD_CS, OUTPUT);
  pinMode(PIN_EPD_DC, OUTPUT);
  pinMode(PIN_EPD_RST, OUTPUT);
  pinMode(PIN_EPD_BUSY, INPUT);
  SPI.begin(PIN_EPD_SCLK, /*MISO*/ -1, PIN_EPD_MOSI, PIN_EPD_CS);
  sleep_display.init(0, false, 50, false);
  sleep_display.setRotation(3);
  sleep_display.setFullWindow();
  sleep_display.setTextColor(GxEPD_BLACK);

  sleep_display.firstPage();
  do {
    sleep_display.fillScreen(GxEPD_WHITE);
    const int16_t W = sleep_display.width();
    const int16_t H = sleep_display.height();

    // Outer frame
    sleep_display.drawRect(12, 12, W - 24, H - 24, GxEPD_BLACK);
    sleep_display.drawRect(14, 14, W - 28, H - 28, GxEPD_BLACK);

    sleep_display.setFont(&FreeSansBold24pt7b);
    const char *title = "BikeHUD";
    int16_t x1, y1;
    uint16_t tw, th;
    sleep_display.getTextBounds(title, 0, 0, &x1, &y1, &tw, &th);
    sleep_display.setCursor((W - (int16_t)tw) / 2 - x1, H / 2 - 40);
    sleep_display.print(title);

    sleep_display.drawFastHLine(W / 4, H / 2 - 10, W / 2, GxEPD_BLACK);

    sleep_display.setFont(&FreeSansBold18pt7b);
    const char *sleeping = "Sleeping";
    sleep_display.getTextBounds(sleeping, 0, 0, &x1, &y1, &tw, &th);
    sleep_display.setCursor((W - (int16_t)tw) / 2 - x1, H / 2 + 30);
    sleep_display.print(sleeping);

    sleep_display.setFont(&FreeSansBold12pt7b);
    const char *hint = "hold power to wake";
    sleep_display.getTextBounds(hint, 0, 0, &x1, &y1, &tw, &th);
    sleep_display.setCursor((W - (int16_t)tw) / 2 - x1, H / 2 + 70);
    sleep_display.print(hint);
  } while (sleep_display.nextPage());

  sleep_display.hibernate();
}

} // namespace

void power_enter_sleep() {
  Serial.println("[power] entering deep sleep");
  Serial.flush();

  // Stop BLE so we don't leave the radio half-on.
#if !defined(BIKE_HUD_DEMO)
  NimBLEDevice::deinit(true);
#endif

  draw_sleep_splash();

  // Wait for release so the still-held button does not wake us immediately.
  while (digitalRead(PIN_BTN_POWER) == LOW) {
    delay(20);
  }
  delay(80);

  // GPIO3 active-low power button → wake on LOW.
  // ESP32-C3: use gpio wakeup API (not classic ext0).
  const uint64_t mask = 1ULL << PIN_BTN_POWER;
  esp_deep_sleep_enable_gpio_wakeup(mask, ESP_GPIO_WAKEUP_GPIO_LOW);

  esp_deep_sleep_start();
  // never returns
  while (true) {
  }
}
