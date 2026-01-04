#include "sd_card.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/sdspi_host.h"

static const char *TAG = "sd_card";
static bool s_is_mounted = false;

// Pin Definitions from docs/t4s3pins.txt
#define SD_PIN_MOSI 2
#define SD_PIN_MISO 4
#define SD_PIN_CLK  3
#define SD_PIN_CS   1

// Use SPI3_HOST to avoid conflict with Display on SPI2_HOST
#define SD_SPI_HOST SPI3_HOST 
#define MOUNT_POINT "/sdcard"

esp_err_t sd_card_init(void) {
    if (s_is_mounted) {
        ESP_LOGW(TAG, "SD Card already mounted");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Initializing SD Card...");

    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024
    };

    sdmmc_card_t *card;
    const char mount_point[] = MOUNT_POINT;

    ESP_LOGI(TAG, "Initializing SPI bus for SD Card...");
    spi_bus_config_t bus_cfg = {
        .mosi_io_num = SD_PIN_MOSI,
        .miso_io_num = SD_PIN_MISO,
        .sclk_io_num = SD_PIN_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 32768, // Increased to 32KB for better throughput
    };
    
    // Initialize the SPI bus
    esp_err_t ret = spi_bus_initialize(SD_SPI_HOST, &bus_cfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize SPI bus: %s", esp_err_to_name(ret));
        return ret;
    }

    // This initializes the slot without initializing the bus (since we just did it)
    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.slot = SD_SPI_HOST;

    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = SD_PIN_CS;
    slot_config.host_id = host.slot;

    ESP_LOGI(TAG, "Mounting filesystem...");
    ret = esp_vfs_fat_sdspi_mount(mount_point, &host, &slot_config, &mount_config, &card);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount filesystem. If you want the card to be formatted, set the EXAMPLE_FORMAT_IF_MOUNT_FAILED menuconfig option.");
        } else {
            ESP_LOGE(TAG, "Failed to initialize the card (%s). Make sure SD card lines have pull-up resistors in place.", esp_err_to_name(ret));
        }
        return ret;
    }
    
    ESP_LOGI(TAG, "Filesystem mounted at %s", mount_point);
    sdmmc_card_print_info(stdout, card);
    s_is_mounted = true;
    
    return ESP_OK;
}

bool sd_card_is_mounted(void) {
    return s_is_mounted;
}
