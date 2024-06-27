#pragma once

#include "esphome/components/json/json_util.h"
#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"

#undef yield
#undef millis
#undef micros
#undef delay
#undef delayMicroseconds

#include "rtl_433_ESP.h"

namespace esphome {
namespace neptune_r900_sensor {

class NeptuneR900Sensor : public sensor::Sensor, public PollingComponent {
  public:  
    void set_consumption(sensor::Sensor *consumption) { consumption_ = consumption; }
    void set_nouse(sensor::Sensor *nouse) { nouse_ = nouse; }
    void set_backflow(sensor::Sensor *backflow) { backflow_ = backflow; }
    void set_meterid(uint32_t id) { id_ = id; }

    void setup() override;
    void loop() override;
    void update() override;
    void dump_config() override;

  protected:
    sensor::Sensor *consumption_;
    sensor::Sensor *nouse_;
    sensor::Sensor *backflow_;
    uint32_t id_;

  private:
    char buffer_[512];

    rtl_433_ESP rf_;

    static NeptuneR900Sensor* instance_;

    static void process_dispatch(char* msg) {
      if (instance_ != nullptr) instance_->process(msg);
      else ESP_LOGD("dispatch", "Process_dispatch called with null instance");
    }

    void process(char* msg);
    void process_json(JsonObject root);
    static void status_task(void *params);
};

} //namespace neptune_r900_sensor
} //namespace esphome