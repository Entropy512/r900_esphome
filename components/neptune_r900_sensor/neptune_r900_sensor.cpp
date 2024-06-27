#include "esphome/core/log.h"
#include "neptune_r900_sensor.h"

namespace esphome {
namespace neptune_r900_sensor {

static const char *TAG = "neptune_r900_sensor.sensor";

NeptuneR900Sensor* NeptuneR900Sensor::instance_ = nullptr;

void NeptuneR900Sensor::setup() {
  instance_ = this;
  ESP_LOGD(TAG, "RF module frequency is %f", RF_MODULE_FREQUENCY);
  ESP_LOGD(TAG, "RF module receiver GPIO is %d", RF_MODULE_RECEIVER_GPIO);
  rf_.initReceiver(RF_MODULE_RECEIVER_GPIO, RF_MODULE_FREQUENCY);
  rf_.setCallback(&NeptuneR900Sensor::process_dispatch, buffer_, sizeof(buffer_));
  rf_.enableReceiver();
}

void NeptuneR900Sensor::loop() {
  rf_.loop();
}

void NeptuneR900Sensor::update() {
    xTaskCreate(NeptuneR900Sensor::status_task, "status_task", 8192, (void*) this, 1, NULL);
}

void NeptuneR900Sensor::dump_config() {
    ESP_LOGCONFIG(TAG, "Neptune R900 sensor");
}

void NeptuneR900Sensor::status_task(void *params) {
  NeptuneR900Sensor *this_sensor = (NeptuneR900Sensor *) params;
  this_sensor->rf_.getStatus();
  vTaskDelete(NULL);
}

void NeptuneR900Sensor::process(char* msg) {
  ESP_LOGD(TAG, "Received msg: %s", msg);
  json::parse_json(msg, [this](JsonObject doc) {
    process_json(doc);
    return true;
  });
}

void NeptuneR900Sensor::process_json(JsonObject doc) {
  const char* model = doc["model"];
  if (strcmp(model, "status") == 0) return;
  else if (strcmp(model, "Neptune-R900") == 0) {
    if (doc["id"] != id_) return;

    if (this->consumption_ != nullptr)
      this->consumption_->publish_state((float) doc["consumption"]/10.0f);
    if (this->nouse_ != nullptr)
      this->nouse_->publish_state(doc["nouse"]);
    if (this->backflow_ != nullptr)
      this->backflow_->publish_state(doc["backflow"]);
  }
}

} //namespace neptune_r900_sensor
} //namespace esphome