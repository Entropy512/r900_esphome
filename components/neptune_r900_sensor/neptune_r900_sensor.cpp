#include "neptune_r900_sensor.h"
#include "cc1101_r900.h"
#include "esphome/core/log.h"
#include <algorithm>

namespace esphome {
namespace neptune_r900_sensor {

static const char *const TAG = "neptune_r900_sensor";
static constexpr uint32_t IDLE_US = 200;
static constexpr size_t MIN_RUNS = 60;
static constexpr size_t MAX_RUNS = 600;

void NeptuneR900Sensor::setup() {
  cs_pin_->setup(); gdo0_pin_->setup(); sck_pin_->setup(); mosi_pin_->setup(); miso_pin_->setup();
  configurePins(cs_pin_->get_pin(), gdo0_pin_->get_pin(), sck_pin_->get_pin(), mosi_pin_->get_pin(), miso_pin_->get_pin());
  if (!cc1101::begin(frequency_hz_)) {
    ESP_LOGE(TAG, "CC1101 not found; check wiring and SPI pin configuration");
    this->mark_failed();
    return;
  }
  ESP_LOGI(TAG, "CC1101 initialized at %.5f MHz; accepting meter ID %lu", frequency_hz_ / 1e6,
           static_cast<unsigned long>(meter_id_));
  if (!this->rmt_begin_()) {
    ESP_LOGE(TAG, "Failed to initialize RMT receive capture");
    this->mark_failed();
  }
}

bool NeptuneR900Sensor::rmt_begin_() {
  rmt_config_t config = RMT_DEFAULT_CONFIG_RX(static_cast<gpio_num_t>(gdo0_pin_->get_pin()), rmt_channel_);
  config.clk_div = 80;  // 1 us ticks on the 80 MHz APB clock
  config.mem_block_num = 4;
  config.rx_config.filter_en = true;
  config.rx_config.filter_ticks_thresh = 255;
  config.rx_config.idle_threshold = IDLE_US;
  esp_err_t err = rmt_config(&config);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "rmt_config failed: %s", esp_err_to_name(err));
    return false;
  }
  err = rmt_driver_install(rmt_channel_, 4096, 0);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "rmt_driver_install failed: %s", esp_err_to_name(err));
    return false;
  }
  rmt_get_ringbuf_handle(rmt_channel_, &ringbuf_);
  if (ringbuf_ == nullptr) return false;
  err = rmt_rx_start(rmt_channel_, true);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "rmt_rx_start failed: %s", esp_err_to_name(err));
    return false;
  }
  return true;
}

void NeptuneR900Sensor::loop() {
  if (this->is_failed() || ringbuf_ == nullptr) return;
  size_t len = 0;
  auto *items = static_cast<rmt_item32_t *>(xRingbufferReceive(ringbuf_, &len, 0));
  if (items != nullptr) {
    this->process_frame_(items, len);
    vRingbufferReturnItem(ringbuf_, items);
  }

  const uint32_t now = millis();
  if (now - last_stats_ms_ >= 10000) {
    last_stats_ms_ = now;
    const uint8_t state = cc1101::readReg(cc1101::ST_MARCSTATE) & 0x1F;
    if (state != 13) {
      cc1101::strobe(cc1101::SIDLE);
      cc1101::strobe(cc1101::SRX);
    }
    ESP_LOGD(TAG, "frames=%lu short=%lu rejected=%lu decoded=%lu ignored_meter=%lu RSSI=%d dBm MARCSTATE=%u",
             static_cast<unsigned long>(frames_), static_cast<unsigned long>(short_frames_),
             static_cast<unsigned long>(rejected_), static_cast<unsigned long>(decoded_),
             static_cast<unsigned long>(ignored_), cc1101::rssiDbm(), state);
  }
}

void NeptuneR900Sensor::process_frame_(rmt_item32_t *items, size_t len) {
  const size_t count = len / sizeof(rmt_item32_t);
  static R900Run runs[MAX_RUNS];
  size_t run_count = 0;
  for (size_t i = 0; i < count && run_count + 2 <= MAX_RUNS; i++) {
    if (items[i].duration0 == 0) break;
    runs[run_count++] = {static_cast<uint16_t>(items[i].duration0), static_cast<uint8_t>(items[i].level0)};
    if (items[i].duration1 == 0) break;
    runs[run_count++] = {static_cast<uint16_t>(items[i].duration1), static_cast<uint8_t>(items[i].level1)};
  }
  frames_++;
  if (run_count < MIN_RUNS) {
    short_frames_++;
    return;
  }

  R900Msg message{};
  uint8_t chips[R900_MAX_CHIPS];
  r900_runs_to_chips(runs, run_count, chips, invert_);
  if (!r900_decode_chips(chips, R900_MAX_CHIPS, message)) {
    rejected_++;
    return;
  }
  decoded_++;
  if (message.id != meter_id_) {
    ignored_++;
    ESP_LOGV(TAG, "Ignoring packet from meter ID %lu (configured ID %lu)",
             static_cast<unsigned long>(message.id), static_cast<unsigned long>(meter_id_));
    return;
  }

  // Consumption is encoded in hundredths of a gallon by this meter; publish gallons.
  if (consumption_sensor_ != nullptr) consumption_sensor_->publish_state(message.consumption / 100.0f);
  if (nouse_sensor_ != nullptr) nouse_sensor_->publish_state(message.nouse);
  if (backflow_sensor_ != nullptr) backflow_sensor_->publish_state(message.backflow);
  if (leak_sensor_ != nullptr) leak_sensor_->publish_state(message.leak);
  if (leak_now_sensor_ != nullptr) leak_now_sensor_->publish_state(message.leakNow);
  if (rssi_sensor_ != nullptr) rssi_sensor_->publish_state(cc1101::rssiDbm());
  ESP_LOGD(TAG, "Meter %lu: consumption=%.2f gal nouse=%u backflow=%u leak=%u leak_now=%u RSSI=%d dBm",
           static_cast<unsigned long>(message.id), message.consumption / 100.0f,
           message.nouse, message.backflow, message.leak, message.leakNow, cc1101::rssiDbm());
}

void NeptuneR900Sensor::dump_config() {
  ESP_LOGCONFIG(TAG, "Neptune R900 receiver:");
  ESP_LOGCONFIG(TAG, "  Meter ID: %lu", static_cast<unsigned long>(meter_id_));
  ESP_LOGCONFIG(TAG, "  Frequency: %.5f MHz", frequency_hz_ / 1e6);
  ESP_LOGCONFIG(TAG, "  CS: GPIO%d, GDO0: GPIO%d, SCK: GPIO%d, MOSI: GPIO%d, MISO: GPIO%d",
                cs_pin_->get_pin(), gdo0_pin_->get_pin(), sck_pin_->get_pin(), mosi_pin_->get_pin(), miso_pin_->get_pin());
  ESP_LOGCONFIG(TAG, "  Invert: %s", invert_ ? "true" : "false");
}

}  // namespace neptune_r900_sensor
}  // namespace esphome
