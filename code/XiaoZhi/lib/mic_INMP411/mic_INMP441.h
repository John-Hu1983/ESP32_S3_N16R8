#ifndef INMP441_H
#define INMP441_H

#include <Arduino.h>
#include <driver/i2s.h>

// ------------------- INMP441 Configuration -------------------
#define I2S_WS_PIN GPIO_NUM_5                     // WS/LRCL -> GPIO5
#define I2S_SCK_PIN GPIO_NUM_6                    // SCK/BCLK -> GPIO6
#define I2S_SD_PIN GPIO_NUM_4                     // SD/DOUT -> GPIO4
#define I2S_PORT I2S_NUM_0                        // Use I2S Port 0
#define SAMPLE_RATE 44100                         // 44.1kHz sampling rate (INMP441 default)
#define BITS_PER_SAMPLE I2S_BITS_PER_SAMPLE_32BIT // 32-bit container for 24-bit data
#define BUFFER_SIZE 1024                          // I2C buffer size

// ------------------- INMP441 Audio Data Structure -------------------
struct AudioData {
  int32_t* rawBuffer;      // Raw 32-bit audio data buffer
  int16_t* processedBuffer;// Processed 16-bit audio data buffer
  int samplesRead;         // Number of samples read
  int16_t averageAmplitude;// Average amplitude of samples
  int16_t maxAmplitude;    // Maximum amplitude of samples
  int16_t latestSample;    // Latest processed sample
};

// ------------------- INMP441 MEMS Microphone Class -------------------
class INMP441 {
private:
  i2s_config_t _i2sConfig;   // I2S configuration structure
  i2s_pin_config_t _pinConfig;// I2S pin configuration
  
public:
  // Constructor
  INMP441();
  
  // Initialize INMP441 and I2S driver
  bool begin();
  
  // Read audio data from INMP441
  esp_err_t readAudioData(AudioData& audioData, size_t bufferSize, TickType_t timeout);
  
  // Process raw 32-bit audio data to 16-bit
  void processAudioData(AudioData& audioData);
  
  // Calculate audio statistics (average, max amplitude)
  void calculateAudioStats(AudioData& audioData);
  
  // Clear DMA buffer
  void clearDMABuffer();
};

#endif // INMP441_H
