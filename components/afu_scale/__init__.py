import esphome.codegen as cg

CODEOWNERS = ["@your_github_username"]

# 顶层命名空间，供 sensor 子平台导入使用。
# 本组件没有顶层配置块，只通过 `sensor:` 平台实例化。
afu_scale_ns = cg.esphome_ns.namespace("afu_scale")
