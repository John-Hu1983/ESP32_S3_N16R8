#ifndef CH1116_OLED_H
#define CH1116_OLED_H

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>

// ------------------- CH1116 OLED (I2C) Configuration -------------------
#define SCREEN_WIDTH 128 // CH1116 128x64 resolution
#define SCREEN_HEIGHT 64
#define CH1116_I2C_ADDR 0x3C // Default I2C address (try 0x3D if not working)
#define CH1116_RST -1        // Reset pin (set to -1 if not connected)

#define PAGE_SIZE    8
#define XLevelL		   0x02
#define XLevelH		   0x10
#define YLevel       0xB0


// ------------------- I2C Configuration -------------------
#define I2C_SDA_PIN GPIO_NUM_20 // I2C SDA -> GPIO19 (Custom pin)
#define I2C_SCL_PIN GPIO_NUM_19 // I2C SCL -> GPIO20 (Custom pin)
#define I2C_FREQUENCY 400000    // 400kHz I2C frequency

// Color definitions for monochrome OLED
#define BLACK 0
#define WHITE 1

// ------------------- CH1116 OLED Driver Class (I2C + Adafruit GFX) -------------------
class CH1116_OLED : public Adafruit_GFX
{
private:
  uint8_t _i2cAddr;
  int8_t _rstPin;
  uint8_t _buffer[SCREEN_WIDTH * SCREEN_HEIGHT / 8]; // 1024-byte buffer (128x64/8)

  // Send single command to CH1116 (I2C: 0x00 = command register)
  void sendCommand(uint8_t cmd);

  // Reset OLED (if RST pin is connected)
  void reset();

public:
  // Constructor: I2C address + reset pin (optional)
  CH1116_OLED(uint8_t i2cAddr, int8_t rstPin = -1);

  // Initialize CH1116 (critical: specific initialization sequence)
  bool begin();

  // Clear internal buffer (set all bytes to 0)
  void clearDisplay();

  // Update OLED with buffer data (page-wise write)
  void display();

  // Override Adafruit GFX: Draw pixel to buffer (core graphics function)
  void drawPixel(int16_t x, int16_t y, uint16_t color) override;
};

#endif // CH1116_OLED_H
