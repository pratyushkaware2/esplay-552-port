#include "esp_heap_caps.h"
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_lcd_ili9341.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "display.h"

// Pin Cofiguration
#define DISP_SPI_MOSI 23
#define DISP_SPI_CLK 18
#define DISP_SPI_CS 5
#define DISP_SPI_DC 12
#define LCD_BCKL 27

// SPI Parameter
#define SPI_CLOCK_SPEED (60 * 1000 * 1000)
#define LCD_HOST       SPI2_HOST

// The pixel number in horizontal and vertical
#define LCD_H_RES              320
#define LCD_V_RES              240

// Bit number used to represent command and parameter
#define LCD_CMD_BITS           8
#define LCD_PARAM_BITS         8

#define LEDC_TIMER      LEDC_TIMER_0
#define LEDC_MODE       LEDC_LOW_SPEED_MODE
#define LEDC_CHANNEL    LEDC_CHANNEL_0
#define LEDC_DUTY_RES   LEDC_TIMER_13_BIT
#define LEDC_FREQUENCY  (5000)

#define DUTY_MAX ((1<<13)-1)
#define LCD_BACKLIGHT_ON_VALUE 1
esp_lcd_panel_handle_t panel_handle = NULL;

static void backlight_init()
{
    //configure timer0
    ledc_timer_config_t ledc_timer;
    memset(&ledc_timer, 0, sizeof(ledc_timer));

    ledc_timer.duty_resolution = LEDC_DUTY_RES; //set timer counter bit number
    ledc_timer.freq_hz = LEDC_FREQUENCY;                      //set frequency of pwm
    ledc_timer.speed_mode = LEDC_MODE;    //timer mode,
    ledc_timer.timer_num = LEDC_TIMER;            //timer index
    ledc_timer.clk_cfg = LEDC_AUTO_CLK;

    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

    //set the configuration
    ledc_channel_config_t ledc_channel;
    memset(&ledc_channel, 0, sizeof(ledc_channel));

    //set LEDC channel 0
    ledc_channel.channel = LEDC_CHANNEL_0;
    //set the duty for initialization.(duty range is 0 ~ ((2**duty_resolution)-1)
    ledc_channel.duty = (LCD_BACKLIGHT_ON_VALUE) ? 0 : DUTY_MAX;
    //GPIO number
    ledc_channel.gpio_num = LCD_BCKL;
    //GPIO INTR TYPE, as an example, we enable fade_end interrupt here.
    ledc_channel.intr_type = LEDC_INTR_FADE_END;
    //set LEDC mode, from ledc_mode_t
    ledc_channel.speed_mode = LEDC_LOW_SPEED_MODE;
    //set LEDC timer source, if different channel use one timer,
    //the frequency and duty_resolution of these channels should be the same
    ledc_channel.timer_sel = LEDC_TIMER_0;

    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));

    printf("Backlight initialization done.\n");
}

void set_display_brightness(int percent)
{
    if (percent > 100) percent = 100;
    if (percent < 0) percent = 0;

    uint32_t duty = (8191 * percent) / 100;
    ESP_ERROR_CHECK(ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, duty));
    ESP_ERROR_CHECK(ledc_update_duty(LEDC_MODE, LEDC_CHANNEL));
}

void display_draw_pixel(uint16_t x,uint16_t y, uint16_t color){
    uint16_t swapped = (color >> 8) | (color << 8);
    esp_lcd_panel_draw_bitmap(panel_handle, x, y, x + 1, y + 1, &swapped);
}

void display_clear(uint16_t color)
{
    size_t buffer_size = LCD_H_RES *16;
    uint16_t* buffer = (uint16_t*)malloc(buffer_size * sizeof(uint16_t));
    if (buffer == NULL){
        for(int i=0; i < LCD_H_RES * LCD_V_RES; i++){
            display_draw_pixel(i% LCD_H_RES, i/ LCD_H_RES, color);
        }
        return;
    }
    uint16_t swapped = (color >> 8) | (color << 8);
    for (int i = 0; i < buffer_size; i++){
        buffer[i] = swapped;
    }

    for (int y = 0; y < LCD_V_RES; y += 16){
        int height = (y+16>LCD_V_RES) ? (LCD_V_RES - y) : 16;
        esp_lcd_panel_draw_bitmap(panel_handle, 0, y, LCD_H_RES, y + height, buffer);
    }
    free(buffer);
}

void display_init(){
    backlight_init();

    spi_bus_config_t buscfg;
    memset(&buscfg, 0, sizeof(buscfg));
    buscfg.sclk_io_num = DISP_SPI_CLK;
    buscfg.mosi_io_num = DISP_SPI_MOSI;
    buscfg.miso_io_num = -1;
    buscfg.quadwp_io_num = -1;
    buscfg.quadhd_io_num = -1;
    buscfg.max_transfer_sz = LCD_H_RES * 16 * sizeof(uint16_t);
    
    ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST, &buscfg, SPI_DMA_CH_AUTO)); 

    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_panel_io_spi_config_t io_config;
    memset(&io_config, 0, sizeof(io_config));
    io_config.dc_gpio_num = DISP_SPI_DC;
    io_config.cs_gpio_num = DISP_SPI_CS;
    io_config.pclk_hz = 40000000;
    io_config.lcd_cmd_bits = LCD_CMD_BITS;
    io_config.lcd_param_bits = LCD_PARAM_BITS;
    io_config.spi_mode = 0;
    io_config.trans_queue_depth = 10;

    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST, &io_config, &io_handle));

    esp_lcd_panel_dev_config_t panel_config;
    memset(&panel_config, 0, sizeof(panel_config));
    panel_config.reset_gpio_num = -1;
    panel_config.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR;
    panel_config.bits_per_pixel = 16;
    
    ESP_ERROR_CHECK(esp_lcd_new_panel_ili9341(io_handle, &panel_config, &panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(panel_handle, true));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));
    set_display_brightness(100);
}
