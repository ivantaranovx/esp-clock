
#include "driver/i2c.h"

#include "i2c.h"
#include "err.h"

#define I2C_SCL GPIO_NUM_5
#define I2C_SDA GPIO_NUM_4
#define I2C_PORT I2C_NUM_0
#define ACK_CHECK_EN true
#define ACK_CHECK_DIS false

esp_err_t i2c_init(void)
{
    gpio_config_t io_conf;
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT_OD;
    io_conf.pin_bit_mask = BIT(I2C_SCL) | BIT(I2C_SDA);
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 1;
    gpio_config(&io_conf);

    for (int i = 0; i < 10; i++)
    {
        gpio_set_level(I2C_SCL, 0);
        os_delay_us(10);
        gpio_set_level(I2C_SCL, 1);
        os_delay_us(10);
    }

    esp_err_t err;
    i2c_config_t conf;
    conf.mode = I2C_MODE_MASTER;
    conf.scl_io_num = I2C_SCL;
    conf.scl_pullup_en = 1;
    conf.sda_io_num = I2C_SDA;
    conf.sda_pullup_en = 1;
    conf.clk_stretch_tick = 300;
    ERR_RET(i2c_driver_install(I2C_PORT, conf.mode));
    ERR_RET(i2c_param_config(I2C_PORT, &conf));

    return ESP_OK;
}

esp_err_t i2c_write(uint8_t addr, uint8_t reg, void *data, size_t data_len)
{
    i2c_cmd_handle_t link = i2c_cmd_link_create();
    i2c_master_start(link);
    i2c_master_write_byte(link, addr | I2C_MASTER_WRITE, ACK_CHECK_EN);
    i2c_master_write_byte(link, reg, ACK_CHECK_EN);
    i2c_master_write(link, data, data_len, ACK_CHECK_EN);
    i2c_master_stop(link);
    int ret = i2c_master_cmd_begin(I2C_PORT, link, 50 / portTICK_RATE_MS);
    i2c_cmd_link_delete(link);
    return ret;
}

esp_err_t i2c_read(uint8_t addr, uint8_t reg, void *data, size_t data_len)
{
    i2c_cmd_handle_t link = i2c_cmd_link_create();
    i2c_master_start(link);
    i2c_master_write_byte(link, addr | I2C_MASTER_WRITE, ACK_CHECK_EN);
    i2c_master_write_byte(link, reg, ACK_CHECK_EN);
    i2c_master_start(link);
    i2c_master_write_byte(link, addr | I2C_MASTER_READ, ACK_CHECK_EN);
    i2c_master_read(link, data, data_len, I2C_MASTER_LAST_NACK);
    i2c_master_stop(link);
    int ret = i2c_master_cmd_begin(I2C_PORT, link, 50 / portTICK_RATE_MS);
    i2c_cmd_link_delete(link);
    return ret;
}
