#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2s.h"
#include "driver/gpio.h"
#include "esp_system.h"
#include "esp_log.h"

#define BUTTON_PIN          39
#define I2S_BCK_IO          19
#define I2S_WS_IO           33
#define I2S_DO_IO           22
#define I2S_DI_IO           23
#define I2S_NUM             I2S_NUM_0
#define MODE_MIC            0
#define MODE_SPK            1
#define SAMPLE_RATE         16000
#define SAMPLE_BITS         16

#define RECORD_TIME         5  // Record time in seconds
#define RECORD_SIZE         (SAMPLE_RATE * SAMPLE_BITS / 8 * RECORD_TIME)

static uint8_t *audio_buffer;

void init_button(void)
{
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = (1ULL << BUTTON_PIN),
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    gpio_config(&io_conf);
}

void init_i2s(int mode)
{
    i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER),
        .sample_rate = SAMPLE_RATE,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 6,
        .dma_buf_len = 60,
    };

    if (mode == MODE_MIC) {
        i2s_config.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_PDM);
    } else {
        i2s_config.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX);
        i2s_config.use_apll = false;
        i2s_config.tx_desc_auto_clear = true;
    }

    i2s_driver_install(I2S_NUM, &i2s_config, 0, NULL);

    i2s_pin_config_t pin_config = {
        .bck_io_num = I2S_BCK_IO,
        .ws_io_num = I2S_WS_IO,
        .data_out_num = I2S_DO_IO,
        .data_in_num = I2S_DI_IO
    };
    i2s_set_pin(I2S_NUM, &pin_config);

    i2s_set_clk(I2S_NUM, SAMPLE_RATE, I2S_BITS_PER_SAMPLE_16BIT, I2S_CHANNEL_MONO);
}

void record_audio(void)
{
    size_t bytes_read;
    init_i2s(MODE_MIC);
    i2s_read(I2S_NUM, audio_buffer, RECORD_SIZE, &bytes_read, portMAX_DELAY);
    i2s_driver_uninstall(I2S_NUM);
    ESP_LOGI("APP", "Finished recording");
}

void play_audio(void)
{
    size_t bytes_written;
    init_i2s(MODE_SPK);
    i2s_write(I2S_NUM, audio_buffer, RECORD_SIZE, &bytes_written, portMAX_DELAY);
    i2s_driver_uninstall(I2S_NUM);
    ESP_LOGI("APP", "Finished playback");
}

void audio_task(void *pvParameters)
{
    while (1) {
        if (gpio_get_level(BUTTON_PIN) == 0) {
            ESP_LOGI("APP", "Button pressed, start recording");
            record_audio();
            ESP_LOGI("APP", "Button released, start playback");
            play_audio();
        }
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}

void app_main(void)
{
    init_button();
    audio_buffer = heap_caps_malloc(RECORD_SIZE, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (audio_buffer == NULL) {
        ESP_LOGE("APP", "Failed to allocate memory for audio buffer");
        return;
    }

    xTaskCreate(audio_task, "audio_task", 4096, NULL, 5, NULL);
}