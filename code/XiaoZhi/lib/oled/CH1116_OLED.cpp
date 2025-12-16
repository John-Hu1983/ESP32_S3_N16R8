#include "CH1116_OLED.h"

// ------------------- Private Methods -------------------

// Send single command to CH1116 (I2C: 0x00 = command register)
void CH1116_OLED::sendCommand(uint8_t cmd)
{
  Wire.beginTransmission(_i2cAddr);
  Wire.write(0x00); // Command register flag
  Wire.write(cmd);
  Wire.endTransmission();
}

// Reset OLED (if RST pin is connected)
void CH1116_OLED::reset()
{
  if (_rstPin >= 0)
  {
    pinMode(_rstPin, OUTPUT);
    digitalWrite(_rstPin, LOW);
    delay(10);
    digitalWrite(_rstPin, HIGH);
    delay(10);
  }
}

// ------------------- Public Methods -------------------

// Constructor: I2C address + reset pin (optional)
CH1116_OLED::CH1116_OLED(uint8_t i2cAddr, int8_t rstPin) : Adafruit_GFX(SCREEN_WIDTH, SCREEN_HEIGHT)
{
  _i2cAddr = i2cAddr;
  _rstPin = rstPin;
}

// Initialize CH1116 (critical: specific initialization sequence)
bool CH1116_OLED::begin()
{
  // Initialize I2C with explicit pins
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  Wire.setClock(I2C_FREQUENCY);

  delay(100); // Wait for OLED to power up
  // Reset OLED
  reset();

  // ------------------- CH1116 Core Initialization Commands -------------------
  sendCommand(0xAE);    // Turn off display (sleep mode)
  sendCommand(XLevelL); // Set column address low nibble
  sendCommand(0x10);    // Set column address high nibble (0x10)
  sendCommand(0x40);    // Set display start line (0x40 = line 0)
  sendCommand(0x81);    // Set contrast control
  sendCommand(0xCF);    // Contrast value (0x00~0xFF, adjust for brightness)
  sendCommand(0xA1);    // Segment re-map (0xA0=normal, 0xA1=flip horizontal)
  sendCommand(0xA6);    // Normal display (0xA6=normal, 0xA7=inverse)
  sendCommand(0xA8);    // Set multiplex ratio (64 lines)
  sendCommand(0x3F);    // 0x3F = 64 lines (for 128x64)
  sendCommand(0xC8);    // COM scan direction (0xC8=flip vertical, 0xC0=normal)
  sendCommand(0xD3);    // Set display offset
  sendCommand(0x00);    // No offset
  sendCommand(0xD5);    // Set clock divide ratio
  sendCommand(0x80);    // Default ratio (1:1)
  sendCommand(0xD9);    // Set pre-charge period
  sendCommand(0xF1);    // Pre-charge for 3.3V operation
  sendCommand(0xDA);    // Set COM pin config
  sendCommand(0x12);    // For 128x64 display
  sendCommand(0xDB);    // Set VCOMH level
  sendCommand(0x40);    // 0.77xVcc (default)
  sendCommand(0x8D);    // Enable charge pump (required for 3.3V)
  sendCommand(0x14);    // 0x14=enable, 0x10=disable
  sendCommand(0xAF);    // Turn on display (wake up)

  // Clear buffer and update display
  delay(100);
  clearDisplay();
  display();

  return true;
}

// Clear internal buffer (set all bytes to 0)
void CH1116_OLED::clearDisplay()
{
  memset(_buffer, 0, sizeof(_buffer));
}

// Update OLED with buffer data (page-wise write)
void CH1116_OLED::display()
{
  for (uint8_t page = 0; page < 8; page++)
  {                           // 8 pages (64 rows / 8)
    sendCommand(0xB0 + page); // Set page address (0xB0~0xB7)
    sendCommand(XLevelL);     // Column low nibble (0x02) - Shift right by 2 columns
    sendCommand(0x10);        // Column high nibble (0x10)

    // Write 126 bytes for the page (I2C data batch) - fits perfectly
    Wire.beginTransmission(_i2cAddr);
    Wire.write(0x40);                       // Data register flag
    for (uint8_t col = 0; col < 128; col++) // Changed from 128 to 126
    {
      Wire.write(_buffer[page * 128 + col]);
    }
    Wire.endTransmission();
  }
}

// Override Adafruit GFX: Draw pixel to buffer (core graphics function)
void CH1116_OLED::drawPixel(int16_t x, int16_t y, uint16_t color)
{
  // Skip out-of-bounds pixels
  if (x < 0 || x >= SCREEN_WIDTH || y < 0 || y >= SCREEN_HEIGHT)
  {
    return;
  }

  // Calculate page and bit position (CH1116 page-wise RAM)
  uint8_t page = y / 8;
  uint8_t bit = y % 8;
  uint16_t index = page * 128 + x;

  if (color)
  {
    _buffer[index] |= (1 << bit); // Turn pixel on
  }
  else
  {
    _buffer[index] &= ~(1 << bit); // Turn pixel off
  }
}
