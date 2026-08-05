import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor, ble_client, esp32_ble_tracker
from esphome.const import (
    DEVICE_CLASS_WEIGHT,
    DEVICE_CLASS_EMPTY,
    STATE_CLASS_MEASUREMENT,
    UNIT_KILOGRAM,
    UNIT_OHM,
    UNIT_PERCENT,
)

from .. import afu_scale_ns

CODEOWNERS = ["@your_github_username"]
DEPENDENCIES = ["ble_client", "esp32_ble_tracker"]

AFUScale = afu_scale_ns.class_(
    "AFUScale",
    sensor.Sensor,
    cg.Component,
    ble_client.BLEClientNode,
    esp32_ble_tracker.ESPBTDeviceListener,
)

CONF_IMPEDANCE = "impedance"
CONF_STABLE = "stable"
CONF_BMI = "bmi"
CONF_BODY_FAT = "body_fat"
CONF_WATER = "water"
CONF_MUSCLE = "muscle"
CONF_PROTEIN = "protein"
CONF_BONE = "bone"

# BIA 计算所需个人参数
CONF_HEIGHT_CM = "height_cm"
CONF_SEX = "sex"
CONF_AGE = "age"

# 主传感器（AFUScale 自身）发布体重，可选子传感器发布阻抗与身体指标
CONFIG_SCHEMA = (
    sensor.sensor_schema(
        AFUScale,
        unit_of_measurement=UNIT_KILOGRAM,
        accuracy_decimals=2,
        device_class=DEVICE_CLASS_WEIGHT,
        state_class=STATE_CLASS_MEASUREMENT,
    )
    .extend(
        {
            cv.Optional(CONF_IMPEDANCE): sensor.sensor_schema(
                unit_of_measurement=UNIT_OHM,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_EMPTY,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_STABLE): sensor.sensor_schema(
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_EMPTY,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_BMI): sensor.sensor_schema(
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_EMPTY,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_BODY_FAT): sensor.sensor_schema(
                unit_of_measurement=UNIT_PERCENT,
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_EMPTY,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_WATER): sensor.sensor_schema(
                unit_of_measurement=UNIT_PERCENT,
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_EMPTY,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_MUSCLE): sensor.sensor_schema(
                unit_of_measurement=UNIT_KILOGRAM,
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_WEIGHT,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_PROTEIN): sensor.sensor_schema(
                unit_of_measurement=UNIT_PERCENT,
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_EMPTY,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_BONE): sensor.sensor_schema(
                unit_of_measurement=UNIT_KILOGRAM,
                accuracy_decimals=2,
                device_class=DEVICE_CLASS_WEIGHT,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            # 个人参数（BIA 计算必需）
            cv.Optional(CONF_HEIGHT_CM, default=170.0): cv.float_,
            cv.Optional(CONF_SEX, default="male"): cv.enum(
                {"male": "male", "female": "female"}, lower=True
            ),
            cv.Optional(CONF_AGE, default=30): cv.int_range(min=1, max=120),
        }
    )
    .extend(ble_client.BLE_CLIENT_SCHEMA)
    .extend(esp32_ble_tracker.ESP_BLE_DEVICE_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    var = await sensor.new_sensor(config)
    await cg.register_component(var, config)
    await ble_client.register_ble_node(var, config)

    # 注册到 esp32_ble_tracker，实现广播监听（双保险方案）
    await esp32_ble_tracker.register_ble_device(var, config)

    # 个人参数
    cg.add(var.set_height_cm(config[CONF_HEIGHT_CM]))
    cg.add(var.set_sex_male(config[CONF_SEX] == "male"))
    cg.add(var.set_age(config[CONF_AGE]))

    if CONF_IMPEDANCE in config:
        sens = await sensor.new_sensor(config[CONF_IMPEDANCE])
        cg.add(var.set_impedance_sensor(sens))
    if CONF_STABLE in config:
        sens = await sensor.new_sensor(config[CONF_STABLE])
        cg.add(var.set_stable_sensor(sens))
    if CONF_BMI in config:
        sens = await sensor.new_sensor(config[CONF_BMI])
        cg.add(var.set_bmi_sensor(sens))
    if CONF_BODY_FAT in config:
        sens = await sensor.new_sensor(config[CONF_BODY_FAT])
        cg.add(var.set_body_fat_sensor(sens))
    if CONF_WATER in config:
        sens = await sensor.new_sensor(config[CONF_WATER])
        cg.add(var.set_water_sensor(sens))
    if CONF_MUSCLE in config:
        sens = await sensor.new_sensor(config[CONF_MUSCLE])
        cg.add(var.set_muscle_sensor(sens))
    if CONF_PROTEIN in config:
        sens = await sensor.new_sensor(config[CONF_PROTEIN])
        cg.add(var.set_protein_sensor(sens))
    if CONF_BONE in config:
        sens = await sensor.new_sensor(config[CONF_BONE])
        cg.add(var.set_bone_sensor(sens))

