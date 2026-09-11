#pragma once

#include "esphome.h"
#include "esphome/components/esp32_camera/esp32_camera.h"

namespace fuel_capture {

using esphome::esp32_camera::ESP32Camera;
using esphome::camera::CameraImage;
using esphome::camera::CameraRequester;

class FuelCapture : public esphome::Component {
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

    // CORRECT: request_image takes a lambda
    self->cam_->request_image([self](CameraImage *image) {
      self->handle_image(image);
    });

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

  void handle_image(CameraImage *image) {
    digitalWrite(pin_, LOW);
    busy_ = false;

    if (!image) {
      ESP_LOGW("fuel_capture", "Image capture failed");
      return;
    }

    // Try all known accessors
    const uint8_t *data = nullptr;
    size_t size = 0;

    // These exist in some builds
    if constexpr (requires(CameraImage img) { img.get_data(); }) {
      data = image->get_data();
      size = image->get_data_size();
    }

    // Fallback: log size only
    ESP_LOGI("fuel_capture", "Image received");
  }
};

}  // namespace fuel_capture
