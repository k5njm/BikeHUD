/**
 * XTEink X4 pin map
 * Source of truth: FreeInk BoardConfig XTEINK_X4 + CidVonHighwind sample
 */
#pragma once

// E-ink SPI (shared bus with SD when used)
static const int PIN_EPD_SCLK = 8;
static const int PIN_EPD_MOSI = 10;
static const int PIN_EPD_CS = 21;
static const int PIN_EPD_DC = 4;
static const int PIN_EPD_RST = 5;
static const int PIN_EPD_BUSY = 6;

// microSD (SPI shared; CS only dedicated)
static const int PIN_SD_CS = 12;
static const int PIN_SD_MISO = 7;

// Buttons
// GPIO1 = ADC resistor ladder: Back, Confirm, Left, Right
// GPIO2 = ADC resistor ladder: Volume Up, Volume Down
// GPIO3 = power (digital, active low)
static const int PIN_BTN_ADC1 = 1;
static const int PIN_BTN_ADC2 = 2;
static const int PIN_BTN_POWER = 3;

// Battery power latch MOSFET (CrossPoint / stock X4): drive LOW + hold in deep
// sleep so the rail is cut on battery. Wake is hard-wired power-button pulse.
static const int PIN_PWR_LATCH = 13;

// Battery voltage divider (½ pack on ADC)
static const int PIN_BAT_ADC = 0;

// USB detect (optional)
static const int PIN_USB_DETECT = 20;
