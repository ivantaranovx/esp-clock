
#include "esp_err.h"
#include <stdint.h>

esp_err_t i2c_init(void);
esp_err_t i2c_write(uint8_t addr, uint8_t reg, void *data, size_t data_len);
esp_err_t i2c_read(uint8_t addr, uint8_t reg, void *data, size_t data_len);
