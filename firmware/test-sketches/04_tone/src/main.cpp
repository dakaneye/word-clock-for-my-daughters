#include <Arduino.h>
#include <driver/i2s.h>
#include <math.h>

constexpr int SAMPLE_RATE = 44100;
constexpr int FREQ = 440;   // A4

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("I2S 440 Hz tone test");

  i2s_config_t cfg = {};
  cfg.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX);
  cfg.sample_rate = SAMPLE_RATE;
  cfg.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
  cfg.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
  cfg.communication_format = I2S_COMM_FORMAT_STAND_I2S;
  cfg.intr_alloc_flags = 0;
  cfg.dma_buf_count = 8;
  cfg.dma_buf_len = 256;
  cfg.use_apll = false;
  cfg.tx_desc_auto_clear = true;

  if (i2s_driver_install(I2S_NUM_0, &cfg, 0, nullptr) != ESP_OK) {
    Serial.println("i2s_driver_install FAILED");
    return;
  }

  i2s_pin_config_t pins = {};
  pins.bck_io_num = 26;      // BCLK
  pins.ws_io_num = 25;       // LRC
  pins.data_out_num = 27;    // DIN
  pins.data_in_num = I2S_PIN_NO_CHANGE;

  if (i2s_set_pin(I2S_NUM_0, &pins) != ESP_OK) {
    Serial.println("i2s_set_pin FAILED");
    return;
  }

  Serial.println("I2S running — should hear 440 Hz tone");
}

void loop() {
  static float phase = 0.0f;
  const float step = 2.0f * M_PI * FREQ / SAMPLE_RATE;
  int16_t buf[256];
  for (int i = 0; i < 256; i += 2) {
    int16_t s = (int16_t)(sinf(phase) * 8000);  // modest amplitude, -12 dB
    buf[i]     = s;   // L
    buf[i + 1] = s;   // R (same data; SD floating = stereo-averaged = mono)
    phase += step;
    if (phase > 2.0f * M_PI) phase -= 2.0f * M_PI;
  }
  size_t written = 0;
  i2s_write(I2S_NUM_0, buf, sizeof(buf), &written, portMAX_DELAY);
}
