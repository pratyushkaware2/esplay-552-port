#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "gamepad.h"
#include "esp_log.h"

static const char *TAG = "GAMEPAD";

#define GPIO_L_BTN 36
#define GPIO_R_BTN 34
#define GPIO_MENU 35

#define I2C_SDA_PIN 21
#define I2C_SCL_PIN 22
#define I2C_FREQ 100000 
#define I2C_PORT I2C_NUM_0 
#define PCF8574_ADDR 0x20 

static volatile bool input_task_is_running = false;
static volatile input_gamepad_state gamepad_state;
static uint8_t debounce[GAMEPAD_INPUT_MAX];
static volatile bool input_gamepad_initialized = false;
static SemaphoreHandle_t xSemaphore;

static void i2c_init()
{
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_SDA_PIN,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_io_num = I2C_SCL_PIN,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_FREQ
    };
    i2c_param_config(I2C_PORT, &conf);
    i2c_driver_install(I2C_PORT, conf.mode, 0, 0, 0);

    ESP_LOGI(TAG, "I2C Driver Installed");
}

static uint8_t i2c_read_pcf8574()
{
    uint8_t val = 0xFF;
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);

    i2c_master_write_byte(cmd, (PCF8574_ADDR << 1) | I2C_MASTER_READ, true);
    i2c_master_read_byte(cmd, &val, I2C_MASTER_NACK);

    i2c_master_stop(cmd);

    esp_err_t ret = i2c_master_cmd_begin(I2C_PORT, cmd, 20 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);

    if (ret != ESP_OK) {
        // Log sparingly
        static int err_cnt = 0;
        if (err_cnt++ % 100 == 0) ESP_LOGE(TAG, "I2C Read Failed: %s", esp_err_to_name(ret));
        return 0xFF;
    }
    return val;
}

static void input_task(void *arg)
{
    input_task_is_running = true;

    for (int i = 0; i < GAMEPAD_INPUT_MAX; ++i) debounce[i] = 0xff;

    while (input_task_is_running)
    {
        for (int i = 0; i < GAMEPAD_INPUT_MAX; ++i) debounce[i] <<= 1;

        uint8_t i2c_data = i2c_read_pcf8574();

        int raw_values[GAMEPAD_INPUT_MAX];

        for (int i = 0; i < 8; ++i) raw_values[i] = ((i2c_data>>i)&1)?0:1;

        raw_values[GAMEPAD_INPUT_MENU] = !gpio_get_level((GPIO_MENU));
        raw_values[GAMEPAD_INPUT_L] = !gpio_get_level((GPIO_L_BTN));
        raw_values[GAMEPAD_INPUT_R] = !gpio_get_level((GPIO_R_BTN));

        xSemaphoreTake(xSemaphore, portMAX_DELAY);

        for (int i = 0; i < GAMEPAD_INPUT_MAX; ++i)
        {
            debounce[i] |= raw_values[i] ? 1 : 0;
            uint8_t val = debounce[i] & 0x03;
            switch (val)
            {
            case 0x00: gamepad_state.values[i] = 0; break;
            case 0x03: gamepad_state.values[i] = 1; break;
            default: break;
            }
        }

        xSemaphoreGive(xSemaphore);
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
    vTaskDelete(NULL);
}

void gamepad_read(input_gamepad_state *out_state)
{
    if (!input_gamepad_initialized) return;
    xSemaphoreTake(xSemaphore, portMAX_DELAY);
    *out_state = gamepad_state;
    xSemaphoreGive(xSemaphore);
}

void gamepad_init()
{
    xSemaphore = xSemaphoreCreateMutex();
    if (xSemaphore == NULL) return;

    i2c_init();

    gpio_config_t btn_config;
    btn_config.intr_type = GPIO_INTR_DISABLE;
    btn_config.mode = GPIO_MODE_INPUT;
    btn_config.pin_bit_mask = ((uint64_t)1 << GPIO_MENU) |
                              ((uint64_t)1 << GPIO_L_BTN) |
                              ((uint64_t)1 << GPIO_R_BTN);
    // Correct for Input-Only pins: No internal pullups allowed
    btn_config.pull_up_en = 0;
    btn_config.pull_down_en = 0;
    gpio_config(&btn_config);

    input_gamepad_initialized = true;
    xTaskCreatePinnedToCore(&input_task, "input_task", 2048, NULL, 5, NULL, 1);
    ESP_LOGI(TAG, "Gamepad Init Done");
}
