#pragma once

#include <vector>

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"
#include "esphome/components/ble_client/ble_client.h"

#ifdef USE_ESP32
#include <esp_gattc_api.h>

namespace esphome {
namespace afu_scale {

/// 单个 AFU 体脂秤节点：同时监听 BLE 广播（免连接）并支持 GATT 连接（完整数据）。
class AFUScale : public sensor::Sensor,
                 public Component,
                 public ble_client::BLEClientNode,
                 public esp32_ble_tracker::ESPBTDeviceListener {
 public:
  void dump_config() override;
  void setup() override;
  float get_setup_priority() const override;

  // ---- esp32_ble_tracker::ESPBTDeviceListener ----
  bool parse_device(const esp32_ble_tracker::ESPBTDevice &device) override;

  // ---- 传感器配置 ----
  void set_impedance_sensor(sensor::Sensor *impedance) { this->impedance_sensor_ = impedance; }
  void set_stable_sensor(sensor::Sensor *stable) { this->stable_sensor_ = stable; }
  void set_bmi_sensor(sensor::Sensor *bmi) { this->bmi_sensor_ = bmi; }
  void set_body_fat_sensor(sensor::Sensor *fat) { this->body_fat_sensor_ = fat; }
  void set_water_sensor(sensor::Sensor *water) { this->water_sensor_ = water; }
  void set_muscle_sensor(sensor::Sensor *muscle) { this->muscle_sensor_ = muscle; }
  void set_protein_sensor(sensor::Sensor *protein) { this->protein_sensor_ = protein; }
  void set_bone_sensor(sensor::Sensor *bone) { this->bone_sensor_ = bone; }

  // ---- BIA 计算所需个人参数 ----
  void set_height_cm(float height_cm) { this->height_cm_ = height_cm; }
  void set_sex_male(bool male) { this->male_ = male; }
  void set_age(int age) { this->age_ = age; }

 protected:
  // GATT 回调（由父级 BLEClient 分发）
  void gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                           esp_ble_gattc_cb_param_t *param) override;

  // 解析工具
  bool parse_notify_packet(const uint8_t *data, size_t len);
  bool parse_adv_data_for_packet(const std::vector<uint8_t> &data);
  void apply_measurement(float weight_kg, bool is_stable, float impedance);

  // BIA 身体指标计算（与 Android BodyAlgorithm.kt 一致）
  float compute_bmi_(float weight_kg) const;
  float compute_body_fat_(float weight_kg, float impedance) const;
  float compute_water_(float fat) const;
  float compute_bone_(float weight_kg) const;
  float compute_protein_(float water) const;
  float compute_muscle_(float weight_kg, float fat, float bone) const;

  sensor::Sensor *impedance_sensor_{nullptr};
  sensor::Sensor *stable_sensor_{nullptr};
  sensor::Sensor *bmi_sensor_{nullptr};
  sensor::Sensor *body_fat_sensor_{nullptr};
  sensor::Sensor *water_sensor_{nullptr};
  sensor::Sensor *muscle_sensor_{nullptr};
  sensor::Sensor *protein_sensor_{nullptr};
  sensor::Sensor *bone_sensor_{nullptr};

  // 个人参数
  float height_cm_{165.0f};
  bool male_{true};
  int age_{23};
};

}  // namespace afu_scale
}  // namespace esphome

#endif  // USE_ESP32
