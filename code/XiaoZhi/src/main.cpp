#include <Arduino.h>
#include <driver/i2s.h>
#include <Wire.h>
#include <Adafruit_GFX.h>

// ------------------- INMP441 Configuration (Do NOT modify if wiring is correct) -------------------
#define I2S_WS_PIN GPIO_NUM_5                     // WS/LRCL -> GPIO5
#define I2S_SCK_PIN GPIO_NUM_6                    // SCK/BCLK -> GPIO6
#define I2S_SD_PIN GPIO_NUM_4                     // SD/DOUT -> GPIO4
#define I2S_PORT I2S_NUM_0                        // Use I2S Port 0
#define SAMPLE_RATE 44100                         // 44.1kHz sampling rate (INMP441 default)
#define BITS_PER_SAMPLE I2S_BITS_PER_SAMPLE_32BIT // 32-bit container for 24-bit data
#define BUFFER_SIZE 1024                          // I2C buffer size

// ------------------- CH1116 OLED (I2C) Configuration -------------------
#define SCREEN_WIDTH 128 // CH1116 128x64 resolution
#define SCREEN_HEIGHT 64
#define CH1116_I2C_ADDR 0x3C // Default I2C address (try 0x3D if not working)
#define CH1116_RST -1        // Reset pin (set to -1 if not connected)

// ------------------- I2C Configuration -------------------
#define I2C_SDA_PIN GPIO_NUM_20 // I2C SDA -> GPIO19 (Custom pin)
#define I2C_SCL_PIN GPIO_NUM_19 // I2C SCL -> GPIO20 (Custom pin)
#define I2C_FREQUENCY 400000    // 400kHz I2C frequency

// Color definitions for monochrome OLED
#define BLACK 0
#define WHITE 1

// Global variable for sound wave visualization (circular buffer)
int16_t waveBuffer[SCREEN_WIDTH] = {0};
int waveIndex = 0;

// ------------------- CH1116 OLED Driver Class (I2C + Adafruit GFX) -------------------
class CH1116_OLED : public Adafruit_GFX
{
private:
  uint8_t _i2cAddr;
  int8_t _rstPin;
  uint8_t _buffer[SCREEN_WIDTH * SCREEN_HEIGHT / 8]; // 1024-byte buffer (128x64/8)

  // Send single command to CH1116 (I2C: 0x00 = command register)
  void sendCommand(uint8_t cmd)
  {
    Wire.beginTransmission(_i2cAddr);
    Wire.write(0x00); // Command register flag
    Wire.write(cmd);
    Wire.endTransmission();
  }

  // Reset OLED (if RST pin is connected)
  void reset()
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

public:
  // Constructor: I2C address + reset pin (optional)
  CH1116_OLED(uint8_t i2cAddr, int8_t rstPin = -1) : Adafruit_GFX(SCREEN_WIDTH, SCREEN_HEIGHT)
  {
    _i2cAddr = i2cAddr;
    _rstPin = rstPin;
  }

  // Initialize CH1116 (critical: specific initialization sequence)
  bool begin()
  {
    // Initialize I2C with explicit pins
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
    Wire.setClock(I2C_FREQUENCY);

    // Reset OLED
    reset();

    // ------------------- CH1116 Core Initialization Commands -------------------
    sendCommand(0xAE); // Turn off display (sleep mode)
    sendCommand(0x02); // Set column address low nibble (0x02) - Shift right by 2 columns
    sendCommand(0x10); // Set column address high nibble (0x10)
    sendCommand(0x40); // Set display start line (0x40 = line 0)
    sendCommand(0x81); // Set contrast control
    sendCommand(0xCF); // Contrast value (0x00~0xFF, adjust for brightness)
    sendCommand(0xA1); // Segment re-map (0xA0=normal, 0xA1=flip horizontal)
    sendCommand(0xA6); // Normal display (0xA6=normal, 0xA7=inverse)
    sendCommand(0xA8); // Set multiplex ratio (64 lines)
    sendCommand(0x3F); // 0x3F = 64 lines (for 128x64)
    sendCommand(0xC8); // COM scan direction (0xC8=flip vertical, 0xC0=normal)
    sendCommand(0xD3); // Set display offset
    sendCommand(0x00); // No offset
    sendCommand(0xD5); // Set clock divide ratio
    sendCommand(0x80); // Default ratio (1:1)
    sendCommand(0xD9); // Set pre-charge period
    sendCommand(0xF1); // Pre-charge for 3.3V operation
    sendCommand(0xDA); // Set COM pin config
    sendCommand(0x12); // For 128x64 display
    sendCommand(0xDB); // Set VCOMH level
    sendCommand(0x40); // 0.77xVcc (default)
    sendCommand(0x8D); // Enable charge pump (required for 3.3V)
    sendCommand(0x14); // 0x14=enable, 0x10=disable
    sendCommand(0xAF); // Turn on display (wake up)

    // Clear buffer and update display
    clearDisplay();
    display();

    return true;
  }

  // Clear internal buffer (set all bytes to 0)
  void clearDisplay()
  {
    memset(_buffer, 0, sizeof(_buffer));
  }

  // Update OLED with buffer data (page-wise write)
  void display()
  {
    for (uint8_t page = 0; page < 8; page++)
    {                           // 8 pages (64 rows / 8)
      sendCommand(0xB0 + page); // Set page address (0xB0~0xB7)
      sendCommand(0x02);        // Column low nibble (0x02) - Shift right by 2 columns
      sendCommand(0x10);        // Column high nibble (0x10)

      // Write 128 bytes for the page (I2C data batch)
      Wire.beginTransmission(_i2cAddr);
      Wire.write(0x40); // Data register flag
      for (uint8_t col = 0; col < 128; col++)
      {
        Wire.write(_buffer[page * 128 + col]);
      }
      Wire.endTransmission();
    }
  }

  // Override Adafruit GFX: Draw pixel to buffer (core graphics function)
  void drawPixel(int16_t x, int16_t y, uint16_t color)
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
};

// Create CH1116 OLED object
CH1116_OLED display(CH1116_I2C_ADDR, CH1116_RST);

// ------------------- INMP441 I2S Initialization -------------------
void i2s_config_init()
{
  i2s_config_t i2s_config = {
      .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX), // Master + Receive mode
      .sample_rate = SAMPLE_RATE,
      .bits_per_sample = BITS_PER_SAMPLE,
      .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT, // Left channel (L/R grounded)
      .communication_format = (i2s_comm_format_t)(I2S_COMM_FORMAT_STAND_MSB),
      .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1, // Interrupt priority
      .dma_buf_count = 8,                       // DMA buffer count
      .dma_buf_len = BUFFER_SIZE,               // DMA buffer length
      .use_apll = true,                         // Accurate clock for audio
      .tx_desc_auto_clear = false,
      .fixed_mclk = 0};

  i2s_pin_config_t pin_config = {
      .bck_io_num = I2S_SCK_PIN,
      .ws_io_num = I2S_WS_PIN,
      .data_out_num = I2S_PIN_NO_CHANGE, // No TX (only RX)
      .data_in_num = I2S_SD_PIN};

  // Install I2S driver
  esp_err_t err = i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
  if (err != ESP_OK)
  {
    Serial.printf("I2S Driver Install Failed: %d\n", err);
    return;
  }

  // Set I2S pins
  i2s_set_pin(I2S_PORT, &pin_config);
  i2s_zero_dma_buffer(I2S_PORT); // Clear DMA buffer
}

// ------------------- Update OLED with Audio Data -------------------
void update_oled(int16_t avg_amp, int16_t max_amp, int16_t sample)
{
  display.clearDisplay(); // Clear buffer

  // ------------------- Text Display (Top Section) -------------------
  display.setTextSize(1);      // Text size: 1 (8x8 pixels)
  display.setTextColor(WHITE); // White text (color = 1)

  // Line 1: Average Amplitude
  display.setCursor(0, 0);
  display.print(F("Avg Amp: "));
  display.print(avg_amp);

  // Line 2: Maximum Amplitude
  display.setCursor(0, 10);
  display.print(F("Max Amp: "));
  display.print(max_amp);

  // Line 3: Latest Sample
  display.setCursor(0, 20);
  display.print(F("Sample: "));
  display.print(sample);

  // ------------------- Sound Wave Visualization (Bottom Section) -------------------
  // Normalize amplitude to OLED height (0 to 32 = half of 64 rows)
  int16_t normalizedAmp = map(avg_amp, 0, 32767, 0, SCREEN_HEIGHT / 2);
  waveBuffer[waveIndex] = normalizedAmp;      // Update circular buffer
  waveIndex = (waveIndex + 1) % SCREEN_WIDTH; // Circular index

  // Draw vertical lines for sound wave (base Y = 40)
  int waveY = 40;
  for (int x = 0; x < SCREEN_WIDTH; x++)
  {
    int currentAmp = waveBuffer[x];
    display.drawLine(x, waveY - currentAmp, x, waveY + currentAmp, WHITE);
  }

  display.display(); // Push buffer to OLED
}

// ------------------- Setup (Initialize Hardware) -------------------
void setup()
{
  // Initialize Serial (115200 baud rate)
  Serial.begin(115200);
  while (!Serial)
  {
    delay(10); // Wait for Serial Monitor to connect
  }

  // Initialize CH1116 OLED
  if (!display.begin())
  {
    Serial.println(F("CH1116 OLED Initialization Failed!"));
    for (;;)
      ; // Halt program if OLED fails
  }

  // Show initial message on OLED
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
  display.print(F("INMP441 + CH1116 Test"));
  display.setCursor(0, 10);
  display.print(F("System Ready!"));
  display.display();
  delay(1000); // Show message for 1 second

  // Initialize INMP441
  Serial.println("\nINMP441 MEMS Microphone Test");
  Serial.printf("I2S Pins: WS=%d, SCK=%d, SD=%d\n", I2S_WS_PIN, I2S_SCK_PIN, I2S_SD_PIN);
  Serial.println("⚠️  INMP441 L/R pin must be connected to GND (Left Channel)!");
  i2s_config_init();
  Serial.println("INMP441 I2S Initialized. Starting Sampling...\n");
}

// ------------------- Main Loop (Audio Sampling & Display) -------------------
void loop()
{
  // Buffer for 32-bit audio data (INMP441 24-bit data in 32-bit container)
  int32_t audio_buffer[BUFFER_SIZE / 4];
  size_t bytes_read = 0;

  // Read I2S data (non-blocking, timeout = 100ms)
  esp_err_t err = i2s_read(I2S_PORT, audio_buffer, sizeof(audio_buffer), &bytes_read, 100 / portTICK_PERIOD_MS);

  if (err == ESP_OK && bytes_read > 0)
  {
    // Calculate number of 32-bit samples (4 bytes per sample)
    int samples = bytes_read / sizeof(int32_t);
    int16_t processed_samples[samples];

    // Process 24-bit data: shift right 8 bits → convert to 16-bit
    for (int i = 0; i < samples; i++)
    {
      processed_samples[i] = (int16_t)(audio_buffer[i] >> 8);
    }

    // Calculate average and maximum amplitude
    int64_t sum = 0;
    int16_t max_amp = 0;
    for (int i = 0; i < samples; i++)
    {
      sum += abs(processed_samples[i]);
      max_amp = max(max_amp, (int16_t)abs(processed_samples[i]));
    }
    int16_t avg_amp = samples > 0 ? sum / samples : 0;
    int16_t latest_sample = samples > 0 ? processed_samples[0] : 0;

    // Serial Output (Debug)
    Serial.printf("Samples: %d | Avg Amp: %d | Max Amp: %d | Data: ", samples, avg_amp, max_amp);
    for (int i = 0; i < min(5, samples); i++)
    { // Print first 5 samples
      Serial.print(processed_samples[i]);
      Serial.print(" ");
    }
    Serial.println();

    // Update OLED with audio data
    update_oled(avg_amp, max_amp, latest_sample);
  }
  else if (err == ESP_ERR_TIMEOUT)
  {
    // Timeout error: show on OLED and Serial
    Serial.println("Error: Audio Read Timeout!");
    display.clearDisplay();
    display.setCursor(0, 0);
    display.print(F("Error: Timeout!"));
    display.display();
  }
  else
  {
    // Other errors: show code
    Serial.printf("Error: Audio Read Failed (Code: %d)\n", err);
    display.clearDisplay();
    display.setCursor(0, 0);
    display.print(F("Error: Read Failed!"));
    display.display();
  }

  delay(100); // Reduce update frequency (prevent OLED/Serial overload)
}