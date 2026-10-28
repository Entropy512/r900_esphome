#pragma once

#include "esphome/core/component.h"
#include "esphome/core/gpio.h"
#include "esphome/components/sensor/sensor.h"
#include "driver/rmt.h"
#include "freertos/ringbuf.h"
#include "r900_core.h"

namespace esphome {
namespace neptune_r900_sensor {

class NeptuneR900Sensor : public Component {
 public:
  void set_meter_id(uint32_t value) { meter_id_ = value; }
  void set_pins(InternalGPIOPin *cs, InternalGPIOPin *gdo0, InternalGPIOPin *sck, InternalGPIOPin *mosi, InternalGPIOPin *miso) {
    cs_pin_ = cs; gdo0_pin_ = gdo0; sck_pin_ = sck; mosi_pin_ = mosi; miso_pin_ = miso;
  }
  void set_frequency(double hz) { frequency_hz_ = hz; }
  void set_invert(bool value) { invert_ = value; }
  void set_consumption_sensor(sensor::Sensor *s) { consumption_sensor_ = s; }
  void set_nouse_sensor(sensor::Sensor *s) { nouse_sensor_ = s; }
  void set_backflow_sensor(sensor::Sensor *s) { backflow_sensor_ = s; }
  void set_leak_sensor(sensor::Sensor *s) { leak_sensor_ = s; }
  void set_leak_now_sensor(sensor::Sensor *s) { leak_now_sensor_ = s; }
  void set_rssi_sensor(sensor::Sensor *s) { rssi_sensor_ = s; }

  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

 protected:
  bool rmt_begin_();
  void process_frame_(rmt_item32_t *items, size_t len);
  uint32_t meter_id_{0};
  InternalGPIOPin *cs_pin_{nullptr}, *gdo0_pin_{nullptr}, *sck_pin_{nullptr}, *mosi_pin_{nullptr}, *miso_pin_{nullptr};
  double frequency_hz_{912071010.0};
  bool invert_{false};
  RingbufHandle_t ringbuf_{nullptr};
  rmt_channel_t rmt_channel_{RMT_CHANNEL_0};
  uint32_t frames_{0}, short_frames_{0}, decoded_{0}, rejected_{0}, ignored_{0};
  uint32_t last_stats_ms_{0};
  sensor::Sensor *consumption_sensor_{nullptr};
  sensor::Sensor *nouse_sensor_{nullptr};
  sensor::Sensor *backflow_sensor_{nullptr};
  sensor::Sensor *leak_sensor_{nullptr};
  sensor::Sensor *leak_now_sensor_{nullptr};
  sensor::Sensor *rssi_sensor_{nullptr};
};

}  // namespace neptune_r900_sensor
}  // namespace esphome
