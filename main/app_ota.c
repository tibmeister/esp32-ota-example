#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_event_loop.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_wifi.h"

static const char *TAG = "ota";

// server certificates
extern const char ca_cert_pem_start[] asm("_binary_ca_pem_start");
extern const char ca_cert_pem_end[] asm("_binary_ca_pem_end");

#define OTA_URL_SIZE 1024

bool bNeedUpdate = false;

static esp_err_t validate_image_header(esp_app_desc_t *new_app_info)
{
    if (new_app_info == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    const esp_partition_t *running = esp_ota_get_running_partition();
    esp_app_desc_t running_app_info;

    if (esp_ota_get_partition_description(running, &running_app_info) == ESP_OK)
    {
        ESP_LOGI(TAG, "Running firmware version: %s", running_app_info.version);
    }

#ifndef CONFIG_SKIP_VERSION_CHECK
    if (memcmp(new_app_info->version, running_app_info.version, sizeof(new_app_info->version)) == 0)
    {
        ESP_LOGW(TAG, "Current running version is the same as available server version. We will not continue the update.");
        bNeedUpdate = false;

        return ESP_FAIL;
    }
#endif

    bNeedUpdate = true;
    return ESP_OK;
}

void ota_task(void *pvParameter)
{
    //Pause for 5 seconds to allow for everything to catch up
    vTaskDelay(5000 / portTICK_PERIOD_MS);

    while (1)
    {
        ESP_LOGI(TAG, "Starting OTA update");

        esp_err_t ota_finish_err = ESP_OK;

        esp_http_client_config_t config = {
            .url = CONFIG_FIRMWARE_UPGRADE_URL,
            .timeout_ms = CONFIG_OTA_RECV_TIMEOUT,
            .cert_pem = (char *)ca_cert_pem_start,
        };

#ifdef CONFIG_FIRMWARE_UPGRADE_URL_FROM_STDIN
        char url_buf[OTA_URL_SIZE];

        if (strcmp(config.url, "FROM_STDIN") == 0)
        {
            example_configure_stdin_stdout();
            fgets(url_buf, OTA_URL_SIZE, stdin);
            int len = strlen(url_buf);
            url_buf[len - 1] = '\0';
            config.url = url_buf;
        }
        else
        {
            ESP_LOGE(TAG, "Configuration mismatch: wrong firmware upgrade image url");
            abort();
        }
#endif

#ifdef CONFIG_SKIP_COMMON_NAME_CHECK
        config.skip_cert_common_name_check = true;
#endif

        esp_https_ota_config_t ota_config = {
            .http_config = &config,
        };

        esp_https_ota_handle_t https_ota_handle = NULL;

        esp_err_t err = esp_https_ota_begin(&ota_config, &https_ota_handle);

        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "ESP HTTPS OTA Begin failed");
            vTaskDelete(NULL);
        }

        esp_app_desc_t app_desc;

        err = esp_https_ota_get_img_desc(https_ota_handle, &app_desc);

        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "esp_https_ota_read_img_desc failed");
            goto ota_end;
        }

        err = validate_image_header(&app_desc);

        if (err != ESP_OK && bNeedUpdate)
        {
            ESP_LOGE(TAG, "image header verification failed");
            goto ota_end;
        }

        if(bNeedUpdate)
        {
            ESP_LOGI(TAG, "Firmware update available, downloading now");

            while (1)
            {
                err = esp_https_ota_perform(https_ota_handle);

                if (err != ESP_ERR_HTTPS_OTA_IN_PROGRESS)
                {
                    break;
                }

                ESP_LOGD(TAG, "Image bytes read: %d", esp_https_ota_get_image_len_read(https_ota_handle));
            }

            if (esp_https_ota_is_complete_data_received(https_ota_handle) != true)
            {
                // the OTA image was not completely received and user can customise the response to this situation.
                ESP_LOGE(TAG, "Complete data was not received.");
            }
        }

    ota_end:
        ota_finish_err = esp_https_ota_finish(https_ota_handle);

        if ((err == ESP_OK) && (ota_finish_err == ESP_OK))
        {
            ESP_LOGI(TAG, "OTA upgrade successful. Rebooting in 10 seconds...");
            vTaskDelay(10000 / portTICK_PERIOD_MS);
            esp_restart();
        }
        else
        {
            if (ota_finish_err == ESP_ERR_OTA_VALIDATE_FAILED)
            {
                ESP_LOGE(TAG, "Image validation failed, image is corrupted");
            }

            if(bNeedUpdate)
            {
                ESP_LOGE(TAG, "OTA upgrade failed %d", ota_finish_err);
            }

            // TODO: decide if we still need to delete this in the future
            //vTaskDelete(NULL);
        }

        // No update was found or needed, or there was some issue
        // Wait for 15 seconds then try again
        vTaskDelay(15000 / portTICK_PERIOD_MS);
    }
}

void ota_init()
{
    esp_err_t err = nvs_flash_init();

    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        // 1.OTA app partition table has a smaller NVS partition size than the non-OTA
        // partition table. This size mismatch may cause NVS initialization to fail.
        // 2.NVS partition contains data in new format and cannot be recognized by this version of code.
        // If this happens, we erase NVS partition and initialize NVS again.
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }

    ESP_ERROR_CHECK(err);

    esp_wifi_set_ps(WIFI_PS_NONE);
    xTaskCreate(&ota_task, "ota_task", 1024 * 10, NULL, 5, NULL);
}
