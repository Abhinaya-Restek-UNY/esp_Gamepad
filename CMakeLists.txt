idf_component_register(SRCS "Gamepad.cpp"
    INCLUDE_DIRS "." "./external/PresistentConfig"
    REQUIRES bluepad32 btstack esp_PresistentConfig
)
