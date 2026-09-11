import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import esp32_camera
from esphome.const import CONF_ID, CONF_PIN

fuel_capture_ns = cg.esphome_ns.namespace("fuel_capture")
FuelCapture = fuel_capture_ns.class_("FuelCapture", cg.Component)

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(FuelCapture),
    cv.Required(CONF_PIN): cv.int_,
    cv.Required("camera_id"): cv.use_id(esp32_camera.ESP32Camera),
})

def to_code(config):
    cam = await cg.get_variable(config["camera_id"])
    var = cg.new_Pvariable(config[CONF_ID], cam, config[CONF_PIN])

    await var
    await cg.register_component(var, config)
