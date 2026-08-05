# AFU 体脂秤 ESPHome 集成

这是一个针对 AFU 系列体脂秤的 ESPHome 自定义组件，使用经典 ESP32 作为中继设备，通过 BLE 从体脂秤读取体重、阻抗、稳定状态，并将数据上报到 Home Assistant。适合希望把非标准 BLE 体脂秤接入 HA 的用户。

本项目基于对 [smart-body-scale-android](https://github.com/maoziban/smart-body-scale-android) 的协议逆向，兼容 AFU-WL-TZ-A1 系列秤。

---

## ✨ 特性

- 兼容 AFU 体脂秤 BLE 广播和 GATT 通信
- 自动读取体重、稳定状态、阻抗
- 计算并上报 BMI、体脂率、水分率、肌肉量、蛋白率、骨量
- 支持中文实体名显示
- 支持 `height_cm`、`sex`、`age` 等个人参数配置
- 兼容 Home Assistant + ESPHome 集成

---

## 🔧 硬件要求

- ESP32 开发板（经典 ESP32 最佳）
- AFU 体脂秤
- ESPHome 配置环境
- Home Assistant

---

## 📁 文件布局

建议把本仓库里的 `components/afu_scale/` 放到 ESPHome 配置目录下：

```text
/config/esphome/
  ├── afu.yaml
  └── components/
      └── afu_scale/
          ├── __init__.py
          ├── sensor/
          │   ├── afu_scale_sensor.h
          │   └── afu_scale_sensor.cpp
```

如果你使用了其他路径，在 `afu.yaml` 里把 `external_components` 的 `path` 改成对应目录即可。

---

## 🧩 配置示例

下面是一个可直接使用的 `afu.yaml` 示例：

```yaml
esphome:
  name: body-scale
  friendly_name: 体脂秤

external_components:
  - source:
      type: local
      path: components

esp32:
  board: esp32dev
  framework:
    type: arduino

wifi:
  ssid: !secret wifi_ssid
  password: !secret wifi_password

api:
  encryption:
    key: "u537D8H8Ks77+mWHgkyXHG+Fxy6PNiec8H1Rb4wuo1g="

ota:
  - platform: esphome

logger:
  level: DEBUG

esp32_ble_tracker:
  scan_parameters:
    interval: 320ms
    window: 160ms
    active: true

ble_client:
  - mac_address: "D0:5C:00:2C:01:D7"
    id: scale_client

sensor:
  - platform: afu_scale
    ble_client_id: scale_client
    name: "体重"
    id: scale_weight
    unit_of_measurement: kg
    accuracy_decimals: 2
    device_class: weight
    height_cm: 165
    sex: male
    age: 23
    impedance:
      name: "电阻抗Impedance"
      id: scale_impedance
      unit_of_measurement: "Ω"
      accuracy_decimals: 0
    stable:
      id: scale_stable
      name: "称重稳定Stable"
      accuracy_decimals: 0
    bmi:
      name: "BMI"
      id: scale_bmi
      accuracy_decimals: 1
    body_fat:
      name: "体脂率Body Fat"
      id: scale_body_fat
      unit_of_measurement: "%"
      accuracy_decimals: 1
    water:
      name: "水分率Water"
      id: scale_water
      unit_of_measurement: "%"
      accuracy_decimals: 1
    muscle:
      name: "肌肉量Muscle"
      id: scale_muscle
      unit_of_measurement: kg
      accuracy_decimals: 1
      device_class: weight
    protein:
      name: "蛋白质率Protein"
      id: scale_protein
      unit_of_measurement: "%"
      accuracy_decimals: 1
    bone:
      name: "骨量Bone"
      id: scale_bone
      unit_of_measurement: kg
      accuracy_decimals: 2
      device_class: weight
```

---

## ⚠️ 关键说明

### 1. 必填参数

以下参数必须填写：

- `height_cm`
- `sex`
- `age`

它们用于 BIA 体脂计算。

### 2. 体重不需要额外配置

体重数据直接从秤上读取，不是通过 YAML 单独写入。

### 3. 中文实体名冲突问题

ESPHome 中的中文实体名在转换为 ASCII 后可能重复，导致多个同类传感器冲突。

示例中给中文名加了唯一后缀：`体重`、`电阻抗Impedance`、`体脂率Body Fat` 等。这样可以避免冲突。

如果你希望 HA 显示纯中文，可以在 Home Assistant 中单独修改实体显示名。

### 4. 日志级别建议

推荐将 `logger.level` 设为 `DEBUG`，不要长期使用 `VERY_VERBOSE`，否则可能导致连接不稳定。

### 5. 头文件默认值说明

`afu_scale_sensor.h` 中的 `height_cm`、`age`、`male` 默认值仅作备用。只要你在 `afu.yaml` 里填写了参数，组件会优先使用 YAML 中的值。

---

## 🚀 使用步骤

1. 将 `components/afu_scale/` 放到 ESPHome 配置目录下。
2. 编辑 `afu.yaml`，修改 `mac_address`、`height_cm`、`sex`、`age`。
3. 使用 `esphome run afu.yaml` 编译并刷写 ESP32。
4. 运行后在 Home Assistant 设备页面确认实体是否创建。
5. 站上秤测试，观察体重和稳定状态是否正常。

---

## 🧪 故障排查

### 1. 只显示 3 个传感器

- 说明当前烧录的固件仍是旧版本。
- 需要重新刷写最新 `afu.yaml`。

### 2. 称重一次后实体消失

- 可能是旧固件未更新。
- 也可能是日志级别过高导致连接不稳定。
- 推荐先把 `logger.level` 降低为 `DEBUG`。

### 3. 出现 `Duplicate sensor entity` 错误

- 说明多个中文名称转换后重复。
- 请给名称添加唯一 ASCII 后缀，或者改成不同的中文 + 数字组合。

### 4. 没有收到阻抗 / BIA 指标

- 请确认 `height_cm`、`sex`、`age` 已填写。
- 确认 `ble_client` 的 `mac_address` 正确且设备在蓝牙范围内。
- 保证已经使用最新固件重新烧写。

---

## 🌐 额外说明

- 体重 / 阻抗数据主要通过 GATT 通知获取，广播监听用于识别设备。
- 本组件会自动发送握手包以获取完整的阻抗数据。
- 个别 AFU 体脂秤会直接广播数据，当前实现以 GATT 为主，如有需要可进一步扩展广播解析。

---

## 🔗 相关项目

- [smart-body-scale-android](https://github.com/maoziban/smart-body-scale-android)
- [smart-body-scale-IOS](https://github.com/maoziban/smart-body-scale-IOS)
