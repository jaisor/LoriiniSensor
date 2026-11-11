#pragma once

// Pin map of the target hardware. Values from https://docs.waveshare.com/ESP32-S3-Touch-LCD-1.69
// ("new" board revision - the one with the model name printed on it; the old one differs in the
// buzzer, RTC interrupt and power latch pins, noted below).

#if defined(WAVESHARE_ESP32S3_TOUCH_LCD_169)

  #define BOARD_NAME "Waveshare ESP32-S3-Touch-LCD-1.69"

  // ST7789V2 1.69" IPS LCD, 240x280, 4-wire SPI (write only). The controller RAM is 240x320 and the
  // panel sits 20 rows in, hence the row offset. Corners are rounded (5mm radius) - keep content
  // away from them, see SCREEN_SAFE_INSET.
  #define LCD_DC    4
  #define LCD_CS    5
  #define LCD_SCK   6
  #define LCD_MOSI  7
  #define LCD_RST   8
  #define LCD_BL    15    // Backlight, active high, PWM dimmable
  #define LCD_WIDTH  240
  #define LCD_HEIGHT 280
  #define LCD_COL_OFFSET 0
  #define LCD_ROW_OFFSET 20
  #define LCD_SPI_HZ 40000000

  // Shared I2C bus: CST816T touch (0x15), QMI8658 IMU (0x6B), PCF85063 RTC (0x51)
  #define I2C_SDA   11
  #define I2C_SCL   10
  #define TP_RST    13
  #define TP_INT    14
  #define TOUCH_I2C_ADDR 0x15
  #define IMU_I2C_ADDR   0x6B
  #define IMU_INT   38
  #define RTC_I2C_ADDR   0x51
  #define RTC_INT   39    // old board: 41

  // Power: pressing PWR powers the board from the battery; firmware must hold SYS_EN high to stay on
  // after the button is released, and pull it low to switch off. SYS_OUT reads the PWR button (low = pressed).
  #define PWR_SYS_EN  41  // old board: 35
  #define PWR_SYS_OUT 40  // old board: 36
  #define BUZZER_PIN  42  // Passive buzzer; keep low when idle. old board: 33
  #define BOOT_BUTTON_PIN 0

  // Battery voltage through a 200K/100K divider: VBAT = VADC * 3
  #define BAT_ADC_PIN 1
  #define BAT_ADC_MULTIPLIER 3.0f

  // Free GPIOs on the extension header: 2, 3, 17, 18 (plus the I2C bus above)

#else
  #error "Unknown board - define WAVESHARE_ESP32S3_TOUCH_LCD_169 (see platformio.ini) or add a pin map here"
#endif
