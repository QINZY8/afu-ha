#include "afu_scale_sensor.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

#include <algorithm>

#ifdef USE_ESP32

namespace esphome {
namespace afu_scale {

static const char *const TAG = "afu_scale";

// 体脂秤 GATT 服务 / 特征 UUID（与 Android 客户端一致）
static const esp32_ble_tracker::ESPBTUUID SERVICE_UUID =
    esp32_ble_tracker::ESPBTUUID::from_uint16(0xFFB0);
static const esp32_ble_tracker::ESPBTUUID NOTIFY_CHAR_UUID =
    esp32_ble_tracker::ESPBTUUID::from_uint16(0xFFB2);

// 报文魔数
static const uint8_t PACKET_MAGIC = 0xAC;
// 稳定标志（data[6] == 0x02 表示数值锁定稳定）
static const uint8_t STABLE_FLAG = 0x02;

// 握手包：FD 37 00 00 00 00 00 00 00 37
static const uint8_t HANDSHAKE[] = {0xFD, 0x37, 0x00, 0x00, 0x00, 0x00,
                                    0x00, 0x00, 0x00, 0x37};

float AFUScale::get_setup_priority() const {
  // 需要在蓝牙跟踪初始化之后
  return setup_priority::AFTER_BLUETOOTH;
}

void AFUScale::setup() {
  ESP_LOGCONFIG(TAG, "Setting up AFU Scale...");
  // 尝试通过 esp32_ble_tracker 注册监听（由 __init__.py 的 esp32_ble_tracker 触发）
}

void AFUScale::dump_config() {
  ESP_LOGCONFIG(TAG, "AFU Scale:");
  ESP_LOGCONFIG(TAG, "  Height: %.0f cm, Sex: %s, Age: %d", this->height_cm_,
                this->male_ ? "male" : "female", this->age_);
  LOG_SENSOR("  ", "Weight (main)", this);
  LOG_SENSOR("  ", "Impedance", this->impedance_sensor_);
  LOG_SENSOR("  ", "Stable", this->stable_sensor_);
  LOG_SENSOR("  ", "BMI", this->bmi_sensor_);
  LOG_SENSOR("  ", "Body Fat", this->body_fat_sensor_);
  LOG_SENSOR("  ", "Water", this->water_sensor_);
  LOG_SENSOR("  ", "Muscle", this->muscle_sensor_);
  LOG_SENSOR("  ", "Protein", this->protein_sensor_);
  LOG_SENSOR("  ", "Bone", this->bone_sensor_);
}

bool AFUScale::parse_device(const esp32_ble_tracker::ESPBTDevice &device) {
  // ---- 广播解析：体脂秤可能通过广播直接推送 0xAC 体重报文 ----
  // 很多 AFU 类体脂秤在 GATT 连接时会被主动断开（reason 0x13），
  // 因此广播数据是获取体重的可靠途径。遍历厂商数据与服务数据，
  // 在数据中搜索以 0xAC 开头的报文并解析。
  bool claimed = false;
  for (auto &md : device.get_manufacturer_datas()) {
    if (parse_adv_data_for_packet(md.data)) {
      claimed = true;
    }
  }
  for (auto &sd : device.get_service_datas()) {
    if (parse_adv_data_for_packet(sd.data)) {
      claimed = true;
    }
  }

  // 识别是否为 AFU 体脂秤（携带 FFB0 服务或名称含 AFU/TZ/WL/A1）
  for (auto &uuid : device.get_service_uuids()) {
    if (uuid == SERVICE_UUID) {
      char addr_buf[MAC_ADDRESS_PRETTY_BUFFER_SIZE];
      ESP_LOGD(TAG, "Scale service found via advertisement (MAC: %s)",
               device.address_str_to(addr_buf));
      claimed = true;
    }
  }

  return claimed;
}

bool AFUScale::parse_adv_data_for_packet(const std::vector<uint8_t> &data) {
  // 在广播数据中搜索 0xAC 开头的报文（可能嵌在数据中间）
  for (size_t i = 0; i < data.size(); i++) {
    if (data[i] == PACKET_MAGIC && (data.size() - i) >= 10) {
      parse_notify_packet(data.data() + i, data.size() - i);
      return true;
    }
  }
  return false;
}

void AFUScale::gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                                   esp_ble_gattc_cb_param_t *param) {
  switch (event) {
    case ESP_GATTC_OPEN_EVT: {
      if (param->open.status == ESP_GATT_OK) {
        ESP_LOGI(TAG, "GATT connection opened");
      } else {
        ESP_LOGW(TAG, "GATT open failed, status=%d", param->open.status);
      }
      break;
    }
    case ESP_GATTC_SEARCH_CMPL_EVT: {
      // 通过父级缓存获取 FFB2 特征（服务发现已完成）
      auto *chr = this->parent()->get_characteristic(SERVICE_UUID, NOTIFY_CHAR_UUID);
      if (chr == nullptr) {
        ESP_LOGW(TAG, "FFB2 notify characteristic not found, scale may not be AFU type");
        break;
      }
      ESP_LOGI(TAG, "FFB2 characteristic found, handle=0x%04x", chr->handle);

      // 注册通知（父级 BLEClientBase 会在 REG_FOR_NOTIFY_EVT 中自动启用 CCCD）
      auto status = esp_ble_gattc_register_for_notify(this->parent()->get_gattc_if(),
                                                      this->parent()->get_remote_bda(), chr->handle);
      if (status != ESP_OK) {
        ESP_LOGW(TAG, "esp_ble_gattc_register_for_notify failed, status=%d", status);
      }

      // 向 FFB0 服务下的所有可写特征发送握手包（与 Android sendHandshake 一致，
      // 它会遍历所有可写特征发送握手包，秤可能要求写入特定特征才推送数据）
      auto *svc = this->parent()->get_service(SERVICE_UUID);
      if (svc != nullptr) {
        for (auto *c : svc->characteristics) {
          if (c->properties & ESP_GATT_CHAR_PROP_BIT_WRITE ||
              c->properties & ESP_GATT_CHAR_PROP_BIT_WRITE_NR) {
            ESP_LOGD(TAG, "Sending handshake to char handle=0x%04x", c->handle);
            c->write_value((uint8_t *) HANDSHAKE, sizeof(HANDSHAKE),
                           ESP_GATT_WRITE_TYPE_RSP);
          }
        }
      } else {
        ESP_LOGW(TAG, "FFB0 service not found in parent cache");
      }
      break;
    }
    case ESP_GATTC_REG_FOR_NOTIFY_EVT: {
      // 通知注册完成，标记节点为已建立
      if (param->reg_for_notify.status == ESP_GATT_OK) {
        this->node_state = esp32_ble_tracker::ClientState::ESTABLISHED;
      }
      break;
    }
    case ESP_GATTC_NOTIFY_EVT: {
      // 收到体重报文（含 0xAC 魔数）
      if (param->notify.is_notify) {
        parse_notify_packet(param->notify.value, param->notify.value_len);
      }
      break;
    }
    case ESP_GATTC_DISCONNECT_EVT: {
      ESP_LOGW(TAG, "GATT disconnected");
      break;
    }
    default:
      break;
  }
}

bool AFUScale::parse_notify_packet(const uint8_t *data, size_t len) {
  // 报文至少 10 字节，且以 0xAC 开头
  if (len < 10 || data[0] != PACKET_MAGIC) {
    ESP_LOGVV(TAG, "Ignored packet, len=%u magic=0x%02X", len, (len > 0) ? data[0] : 0);
    return false;
  }

  // 体重：((data[3]-0x68) * 65536 + data[4]*256 + data[5]) / 1000.0
  int32_t w3 = data[3];
  int32_t w4 = data[4];
  int32_t w5 = data[5];
  int32_t raw_weight = (w3 - 0x68) * 65536 + w4 * 256 + w5;
  float weight_kg = raw_weight < 0 ? 0.0f : raw_weight / 1000.0f;

  // 稳定标志：data[6] == 0x02
  bool is_stable = (data[6] == STABLE_FLAG);

  // 电阻抗：data[8]<<8 | data[9]（Big Endian）
  float impedance = (float) ((data[8] << 8) | data[9]);

  ESP_LOGD(TAG, "Packet: weight=%.2fkg stable=%s impedance=%.0f", weight_kg,
           is_stable ? "YES" : "no", impedance);

  apply_measurement(weight_kg, is_stable, impedance);
  return true;
}

void AFUScale::apply_measurement(float weight_kg, bool is_stable, float impedance) {
  // 过滤无效数据：
  // - 体重 <= 0：称重结束/未站人时秤会推送 0，忽略以免重置 HA 里的体重
  // - 电阻抗 < 500：人离开秤后阻抗会骤降（如 384），此时数据无效
  if (weight_kg <= 0.0f || impedance < 500.0f) {
    ESP_LOGVV(TAG, "Discarding invalid reading: weight=%.2fkg impedance=%.0f", weight_kg, impedance);
    return;
  }

  // 主传感器自身发布体重
  this->publish_state(weight_kg);
  if (this->impedance_sensor_ != nullptr) {
    this->impedance_sensor_->publish_state(impedance);
  }
  if (this->stable_sensor_ != nullptr) {
    this->stable_sensor_->publish_state(is_stable ? 1.0f : 0.0f);
  }

  // 计算并发布 BIA 身体指标（仅当已配置身高/性别/年龄时）
  if (this->height_cm_ > 0.0f) {
    float bmi = this->compute_bmi_(weight_kg);
    float fat = this->compute_body_fat_(weight_kg, impedance);
    float water = this->compute_water_(fat);
    float bone = this->compute_bone_(weight_kg);
    float protein = this->compute_protein_(water);
    float muscle = this->compute_muscle_(weight_kg, fat, bone);

    if (this->bmi_sensor_ != nullptr)
      this->bmi_sensor_->publish_state(bmi);
    if (this->body_fat_sensor_ != nullptr)
      this->body_fat_sensor_->publish_state(fat);
    if (this->water_sensor_ != nullptr)
      this->water_sensor_->publish_state(water);
    if (this->muscle_sensor_ != nullptr)
      this->muscle_sensor_->publish_state(muscle);
    if (this->protein_sensor_ != nullptr)
      this->protein_sensor_->publish_state(protein);
    if (this->bone_sensor_ != nullptr)
      this->bone_sensor_->publish_state(bone);
  }
}

float AFUScale::compute_bmi_(float weight_kg) const {
  float height_m = this->height_cm_ / 100.0f;
  return weight_kg / (height_m * height_m);
}

float AFUScale::compute_body_fat_(float weight_kg, float impedance) const {
  float bmi = this->compute_bmi_(weight_kg);
  float sex_offset = this->male_ ? -10.8f : 0.0f;

  // 阻抗 > 0 时用 BIA 法，否则用估算回退法
  float fat;
  if (impedance > 0.0f) {
    if (this->male_) {
      fat = 0.18f * bmi + 0.012f * this->age_ + 0.018f * impedance - 3.2f;
    } else {
      fat = 0.26f * bmi + 0.011f * this->age_ + 0.020f * impedance - 2.5f;
    }
  } else {
    fat = 1.20f * bmi + 0.23f * this->age_ + sex_offset - 5.4f;
  }
  return std::max(5.0f, std::min(55.0f, fat));
}

float AFUScale::compute_water_(float fat) const {
  return std::max(35.0f, std::min(75.0f, 69.7f - fat * 0.55f));
}

float AFUScale::compute_bone_(float weight_kg) const {
  float bone = weight_kg * (this->male_ ? 0.047f : 0.040f);
  return std::max(1.5f, std::min(5.5f, bone));
}

float AFUScale::compute_protein_(float water) const {
  return std::max(10.0f, std::min(24.0f, 16.0f + (water - 50.0f) * 0.12f));
}

float AFUScale::compute_muscle_(float weight_kg, float fat, float bone) const {
  return std::max(0.0f, weight_kg * (1.0f - fat / 100.0f) - bone);
}

}  // namespace afu_scale
}  // namespace esphome

#endif  // USE_ESP32
