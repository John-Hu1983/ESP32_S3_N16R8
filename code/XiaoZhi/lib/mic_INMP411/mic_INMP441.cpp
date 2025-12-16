#include "mic_INMP441.h"

// ------------------- Constructor -------------------
INMP441::INMP441()
{
  // Initialize I2S configuration
  _i2sConfig = {
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

  // Initialize I2S pin configuration
  _pinConfig = {
      .bck_io_num = I2S_SCK_PIN,
      .ws_io_num = I2S_WS_PIN,
      .data_out_num = I2S_PIN_NO_CHANGE, // No TX (only RX)
      .data_in_num = I2S_SD_PIN};
}

// ------------------- Initialize INMP441 and I2S driver -------------------
bool INMP441::begin()
{
  // Install I2S driver
  esp_err_t err = i2s_driver_install(I2S_PORT, &_i2sConfig, 0, NULL);
  if (err != ESP_OK)
  {
    Serial.printf("I2S Driver Install Failed: %d\n", err);
    return false;
  }

  // Set I2S pins
  err = i2s_set_pin(I2S_PORT, &_pinConfig);
  if (err != ESP_OK)
  {
    Serial.printf("I2S Set Pins Failed: %d\n", err);
    return false;
  }

  // Clear DMA buffer
  i2s_zero_dma_buffer(I2S_PORT);

  return true;
}

// ------------------- Read audio data from INMP441 -------------------
esp_err_t INMP441::readAudioData(AudioData &audioData, size_t bufferSize, TickType_t timeout)
{
  size_t bytesRead = 0;

  // Read I2S data
  esp_err_t err = i2s_read(I2S_PORT, audioData.rawBuffer, bufferSize, &bytesRead, timeout);

  if (err == ESP_OK && bytesRead > 0)
  {
    // Calculate number of 32-bit samples (4 bytes per sample)
    audioData.samplesRead = bytesRead / sizeof(int32_t);
  }
  else
  {
    audioData.samplesRead = 0;
  }

  return err;
}

// ------------------- Process raw 32-bit audio data to 16-bit -------------------
void INMP441::processAudioData(AudioData &audioData)
{
  if (audioData.samplesRead == 0)
    return;

  // Process 24-bit data: shift right 8 bits → convert to 16-bit
  for (int i = 0; i < audioData.samplesRead; i++)
  {
    audioData.processedBuffer[i] = (int16_t)(audioData.rawBuffer[i] >> 8);
  }
}

// ------------------- Calculate audio statistics -------------------
void INMP441::calculateAudioStats(AudioData &audioData)
{
  if (audioData.samplesRead == 0)
  {
    audioData.averageAmplitude = 0;
    audioData.maxAmplitude = 0;
    audioData.latestSample = 0;
    return;
  }

  // Calculate average and maximum amplitude
  int64_t sum = 0;
  int16_t maxAmp = 0;

  for (int i = 0; i < audioData.samplesRead; i++)
  {
    int16_t sample = audioData.processedBuffer[i];
    int16_t absSample = abs(sample);

    sum += absSample;
    if (absSample > maxAmp)
    {
      maxAmp = absSample;
    }
  }

  audioData.averageAmplitude = sum / audioData.samplesRead;
  audioData.maxAmplitude = maxAmp;
  audioData.latestSample = audioData.processedBuffer[0];
}

// ------------------- Clear DMA buffer -------------------
void INMP441::clearDMABuffer()
{
  i2s_zero_dma_buffer(I2S_PORT);
}
