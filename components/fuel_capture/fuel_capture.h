#pragma once

#include "esphome.h"
#include "esphome/components/esp32_camera/esp32_camera.h"

namespace fuel_capture {

using esphome::esp32_camera::ESP32Camera;
using esphome::camera::CameraImage;
using esphome::camera::CameraImageCallback;

class FuelCapture : public esphome::Component, public CameraImageCallback {
 public:
  FuelCapture(ESP32Camera *cam, uint8_t pin)
      : cam_(cam), pin_(pin) {}

  void setup() override {
    pinMode(pin_, OUTPUT);
    digitalWrite(pin_, LOW);
  }

  void loop() override {
    uint32_t now = millis();
    if (!busy_ && now - last_ > 10000) {
      last_ = now;
      start_task();
    }
  }

  // REQUIRED by CameraImageCallback
  void on_image(CameraImage *image) override {
    digitalWrite(pin_, LOW);
    busy_ = false;

    if (!image) {
      ESP_LOGW("fuel_capture", "Image capture failed");
      return;
    }

    uint64_t sum = 0;
    for (auto b : image->data) sum += b;
    float avg = float(sum) / image->data.size();

    ESP_LOGI("fuel_capture", "Brightness = %.2f", avg);
  }

 protected:
  ESP32Camera *cam_;
  uint8_t pin_;
  uint32_t last_{0};
  bool busy_{false};

  static void task(void *param) {
    auto *self = static_cast<FuelCapture *>(param);

    self->busy_ = true;
    digitalWrite(self->pin_, HIGH);

    vTaskDelay(pdMS_TO_TICKS(1000));

    // NEW API: only pass the callback object
    self->cam_->request_image(self);

    vTaskDelete(nullptr);
  }

  void start_task() {
    xTaskCreatePinnedToCore(
        &FuelCapture::task,
        "fuel_capture_task",
        8192,
        this,
        5,
        nullptr,
        1
    );
  }
};

}  // namespace fuel_capture
