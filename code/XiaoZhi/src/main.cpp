#include <Arduino.h>
#include <driver/i2s.h>
#include "CH1116_OLED.h"
#include "mic_INMP441.h"

// Create OLED and INMP441 objects
CH1116_OLED display(CH1116_I2C_ADDR, CH1116_RST);
INMP441 microphone;

// Global variable for sound wave visualization (circular buffer)
int16_t waveBuffer[SCREEN_WIDTH] = {0};
int waveIndex = 0;

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
  Serial.println("⚠️  INMP441 L/R pin must be connected to GND (Left Channel)! ");

  if (!microphone.begin())
  {
    Serial.println(F("INMP441 Initialization Failed!"));
    for (;;)
      ; // Halt program if INMP441 fails
  }

  Serial.println("INMP441 I2S Initialized. Starting Sampling...\n");
}

// ------------------- Main Loop (Audio Sampling & Display) -------------------
void loop()
{
  // Buffer for audio data
  int32_t audio_buffer[BUFFER_SIZE / 4];
  int16_t processed_samples[BUFFER_SIZE / 4];

  // Prepare audio data structure
  AudioData audioData;
  audioData.rawBuffer = audio_buffer;
  audioData.processedBuffer = processed_samples;

  // Read I2S data (non-blocking, timeout = 100ms)
  esp_err_t err = microphone.readAudioData(audioData, sizeof(audio_buffer), 100 / portTICK_PERIOD_MS);

  if (err == ESP_OK && audioData.samplesRead > 0)
  {
    // Process audio data
    microphone.processAudioData(audioData);

    // Calculate audio statistics
    microphone.calculateAudioStats(audioData);

    // Serial Output (Debug)
    Serial.printf("Samples: %d | Avg Amp: %d | Max Amp: %d | Data: ",
                  audioData.samplesRead,
                  audioData.averageAmplitude,
                  audioData.maxAmplitude);
    for (int i = 0; i < min(5, audioData.samplesRead); i++)
    { // Print first 5 samples
      Serial.print(audioData.processedBuffer[i]);
      Serial.print(" ");
    }
    Serial.println();

    // Update OLED with audio data
    update_oled(audioData.averageAmplitude, audioData.maxAmplitude, audioData.latestSample);
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