#include <Arduino.h>
#include <driver/i2s.h>
#include "CH1116_OLED.h"
#include "mic_INMP441.h"
#include "MAX98357.h"

// Create OLED, INMP441, and MAX98357 objects
CH1116_OLED display(CH1116_I2C_ADDR, CH1116_RST);
INMP441 microphone;
MAX98357 amplifier;

// Global variable for sound wave visualization (circular buffer)
int16_t waveBuffer[SCREEN_WIDTH] = {0};
int waveIndex = 0;

// Test mode selection
enum TestMode { TONE_TEST, LOOPBACK_TEST };
TestMode currentMode = TONE_TEST;

// ------------------- Generate Test Tone -------------------
void generateTestTone(int16_t* buffer, size_t length, uint16_t frequency = 440, float volume = 0.5) {
  const float sampleRate = 44100.0;
  const float amplitude = 32767.0 * volume;
  
  for (size_t i = 0; i < length; i++) {
    float t = i / sampleRate;
    float sineWave = sin(2 * PI * frequency * t);
    buffer[i] = (int16_t)(sineWave * amplitude);
  }
}

// ------------------- Update OLED with Audio Data -------------------
void update_oled(int16_t avg_amp, int16_t max_amp, int16_t sample) {
  display.clearDisplay(); // Clear buffer

  // ------------------- Text Display (Top Section) -------------------
  display.setTextSize(1);      // Text size: 1 (8x8 pixels)
  display.setTextColor(WHITE); // White text (color = 1)

  // Line 1: Test Mode
  display.setCursor(0, 0);
  if (currentMode == TONE_TEST) {
    display.print(F("Mode: Test Tone"));
  } else {
    display.print(F("Mode: Loopback"));
  }

  // Line 2: Average Amplitude
  display.setCursor(0, 10);
  display.print(F("Avg Amp: "));
  display.print(avg_amp);

  // Line 3: Maximum Amplitude
  display.setCursor(0, 20);
  display.print(F("Max Amp: "));
  display.print(max_amp);

  // Line 4: Latest Sample
  display.setCursor(0, 30);
  display.print(F("Sample: "));
  display.print(sample);

  // ------------------- Sound Wave Visualization (Bottom Section) -------------------
  // Normalize amplitude to OLED height (0 to 32 = half of 64 rows)
  int16_t normalizedAmp = map(avg_amp, 0, 32767, 0, SCREEN_HEIGHT / 2);
  waveBuffer[waveIndex] = normalizedAmp;      // Update circular buffer
  waveIndex = (waveIndex + 1) % SCREEN_WIDTH; // Circular index

  // Draw vertical lines for sound wave (base Y = 48)
  int waveY = 48;
  for (int x = 0; x < SCREEN_WIDTH; x++) {
    int currentAmp = waveBuffer[x];
    display.drawLine(x, waveY - currentAmp, x, waveY + currentAmp, WHITE);
  }

  display.display(); // Push buffer to OLED
}

// ------------------- Setup (Initialize Hardware) -------------------
void setup() {
  // Initialize Serial (115200 baud rate)
  Serial.begin(115200);
  while (!Serial) {
    delay(10); // Wait for Serial Monitor to connect
  }

  // Initialize CH1116 OLED
  if (!display.begin()) {
    Serial.println(F("CH1116 OLED Initialization Failed!"));
    for (;;); // Halt program if OLED fails
  }

  // Show initial message on OLED
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
  display.print(F("MAX98357 Test Demo"));
  display.setCursor(0, 10);
  display.print(F("Initializing..."));
  display.display();
  delay(1000); // Show message for 1 second

  // Initialize MAX98357 Amplifier
  Serial.println("\nMAX98357 Audio Amplifier Test");
  Serial.printf("I2S Pins: BCLK=%d, LRC=%d, DATA=%d\n", MAX98357_BCLK_PIN, MAX98357_LRC_PIN, MAX98357_DATA_PIN);

  if (!amplifier.begin()) {
    Serial.println(F("MAX98357 Initialization Failed!"));
    for (;;); // Halt program if MAX98357 fails
  }
  Serial.println("MAX98357 I2S Initialized.");

  // Initialize INMP441
  Serial.println("\nINMP441 MEMS Microphone Test");
  Serial.printf("I2S Pins: WS=%d, SCK=%d, SD=%d\n", I2S_WS_PIN, I2S_SCK_PIN, I2S_SD_PIN);
  Serial.println("⚠️  INMP441 L/R pin must be connected to GND (Left Channel)! ");

  if (!microphone.begin()) {
    Serial.println(F("INMP441 Initialization Failed!"));
    for (;;); // Halt program if INMP441 fails
  }
  Serial.println("INMP441 I2S Initialized.");

  Serial.println("\nSystem Ready! Press any key to toggle between test modes.");
  Serial.println("Test Tone: Plays a 440Hz sine wave");
  Serial.println("Loopback: Amplifies microphone input");
}

// ------------------- Main Loop (Audio Test) -------------------
void loop() {
  // Check for serial input to toggle test modes
  if (Serial.available() > 0) {
    Serial.read(); // Clear the input buffer
    currentMode = (currentMode == TONE_TEST) ? LOOPBACK_TEST : TONE_TEST;
    Serial.print(F("\nSwitched to "));
    Serial.println((currentMode == TONE_TEST) ? F("Tone Test Mode") : F("Loopback Test Mode"));
  }

  // Buffer for audio data
  int32_t audio_buffer[BUFFER_SIZE / 4];
  int16_t processed_samples[BUFFER_SIZE / 4];

  if (currentMode == TONE_TEST) {
    // Generate a 440Hz test tone (A4 note)
    generateTestTone(processed_samples, BUFFER_SIZE / 4, 440, 0.3);
    
    // Play the test tone through the amplifier
    amplifier.writeAudioDataWithVolume(processed_samples, BUFFER_SIZE / 4, 0.5);
    
    // Update OLED with tone information
    update_oled(16384, 32767, processed_samples[0]);
  } 
  else {
    // LOOPBACK_TEST mode: Microphone input to amplifier output
    
    // Prepare audio data structure
    AudioData audioData;
    audioData.rawBuffer = audio_buffer;
    audioData.processedBuffer = processed_samples;

    // Read I2S data (non-blocking, timeout = 100ms)
    esp_err_t err = microphone.readAudioData(audioData, sizeof(audio_buffer), 100 / portTICK_PERIOD_MS);

    if (err == ESP_OK && audioData.samplesRead > 0) {
      // Process audio data
      microphone.processAudioData(audioData);

      // Calculate audio statistics
      microphone.calculateAudioStats(audioData);

      // Play processed audio through amplifier (with volume control)
      amplifier.writeAudioDataWithVolume(audioData.processedBuffer, audioData.samplesRead, 0.8);

      // Update OLED with audio data
      update_oled(audioData.averageAmplitude, audioData.maxAmplitude, audioData.latestSample);
    }
    else if (err == ESP_ERR_TIMEOUT) {
      // Timeout error: show on OLED and Serial
      Serial.println("Error: Audio Read Timeout!");
      display.clearDisplay();
      display.setCursor(0, 0);
      display.print(F("Error: Timeout!"));
      display.display();
    }
    else {
      // Other errors: show code
      Serial.printf("Error: Audio Read Failed (Code: %d)\n", err);
      display.clearDisplay();
      display.setCursor(0, 0);
      display.print(F("Error: Read Failed!"));
      display.display();
    }
  }

  delay(100); // Reduce update frequency (prevent OLED/Serial overload)
}