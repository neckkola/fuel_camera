#pragma once

#include "esphome.h"
#include "esphome/components/esp32_camera/esp32_camera.h"
#include "esphome/components/camera/camera.h"

struct CalibrationData {
  double angle;
  double percentage;
};

struct NeedleData {
  double angle;
  double radius;
};

struct ScoreData {
    int angle;
    uint32_t score;
};

static uint32_t now = 0;

namespace fuel_capture {

using esphome::esp32_camera::ESP32Camera;
using esphome::camera::CameraImage;
using esphome::camera::CameraRequester;

double CalculateReading(int needleAngle);
int FindNeedle(const uint8_t *image, uint16_t width, uint16_t height, uint16_t cx, uint16_t cy);

class FuelCapture : public esphome::Component, public esphome::camera::CameraListener {
public:
  FuelCapture(ESP32Camera *cam, uint8_t pin)
      : cam_(cam), pin_(pin) {}

  void setup() override {
    pinMode(pin_, OUTPUT);
    digitalWrite(pin_, LOW);
    
    cam_->add_listener(this);
  }

  void loop() override {
    now = millis();

    if (!busy_ && now - last_ > 60000) {
      last_ = now;
      start_task();
 	  ESP_LOGI("fuel_capture", "Started Fuel Task.");
    }
  }

 protected:
  ESP32Camera *cam_;
  uint8_t pin_;
  uint32_t last_{0};
  bool busy_{false};
  bool registered_{false};

  static void task(void *param) {
    auto *self  = static_cast<FuelCapture *>(param);
    self->busy_ = true;

    digitalWrite(self->pin_, HIGH);
    vTaskDelay(pdMS_TO_TICKS(1000));
    self->cam_->request_image(CameraRequester::IDLE);
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

  void on_camera_image(const std::shared_ptr<CameraImage> &image) {
    ESP_LOGI("fuel_capture", "In Handle Image Fuel Loop.");
	  digitalWrite(pin_, LOW);
    busy_ = false;

    if (!image) {
      ESP_LOGW("fuel_capture", "Image capture failed");
      return;
    }

    uint8_t *data = image->get_data_buffer();
    size_t length = image->get_data_length();

    if (data == nullptr) {
      ESP_LOGE("fuel_capture", "Camera returned a NULL data buffer.");
      return;
    }

    // Fallback: log size only
    ESP_LOGI("fuel_capture", "Image received and FindNeedle started. Size: %d bytes", length);

    int needle_angle = fuel_capture::FindNeedle(data, 640, 480, 350, 272);
    ESP_LOGI("fuel_capture", "FindNeedle completed. Angle = %d degrees.", needle_angle);

    double reading = fuel_capture::CalculateReading(needle_angle);
    ESP_LOGI("fuel_capture", "CalculateReading completed. Reading = %.2f%%.", reading);

    if (reading < 0.0)   { reading = 0.0;   }
    if (reading > 100.0) { reading = 100.0; }

    //this->reading_sensor_->publish_state(static_cast<float>(reading));
    ESP_LOGI("fuel_capture", "Gauge reading published: %.1f%%.", reading);
  }
};

double CalculateReading(int needleAngle)
{
    // Need to reset the angle to be zero based to avoid the sharpe drop at 50 degrees
    // let 100% equal 0 degrees.
    // offest is -31 degrees

    needleAngle = needleAngle - 31;

    std::vector<CalibrationData> data = {
        {83, 0},
        {118, 12.5},
        {155, 25},
        {189, 37.5},
        {223, 50},
        {254, 62.5},
        {290, 75},
        {322, 87.5},
        {360, 100}
    };

    // linear interpolation
    double sumX = 0, sumY = 0, sumXY = 0, sumX2 = 0;

    for (const auto& p : data)
    {
        sumX += p.angle;
        sumY += p.percentage;
        sumXY += p.angle * p.percentage;
        sumX2 += p.angle * p.angle;
    }

    double m = (data.size() * sumXY - sumX * sumY) / (data.size() * sumX2 - sumX * sumX);
    double b = (sumY - m * sumX) / data.size();
    double x = m * needleAngle + b;

    return x;
}
int FindNeedle(const uint8_t *image, uint16_t width, uint16_t height, uint16_t cx, uint16_t cy)
{
    // This function is intended to find the needle in the image.
    // Process is to start at the center, move outside of the needle circle and then
    // scan around for the needle

    constexpr double pi = 3.14159265358979323846; // High precision

    std::vector<ScoreData> candidates;
    uint32_t score = 0;

    for (int a = 0; a <=360; a++)
    {
        auto angle = a * pi * 2 / 360;
        for (int r = 20; r <= 70; r++)
        {
            int x = (int)round(cx + cos(angle) * r);
            int y = (int)round(cy + sin(angle) * r);
            auto pixel = image[y * width + x];
            score     += pixel;
        }
    
        ScoreData candidate = {a, score};
        candidates.push_back(candidate);
        score = 0;
    }

    auto best_candidate = std::min_element(
        candidates.begin(), 
        candidates.end(), 
        [](const ScoreData& a, const ScoreData& b) 
        {
            return a.score < b.score;
        }
    );

    return best_candidate->angle;
}

}  // namespace fuel_capture
