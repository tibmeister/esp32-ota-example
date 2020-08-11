#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_event.h"
#include "esp_system.h"
#include "tcpip_adapter.h"
#include "nvs_flash.h"
#include "sdkconfig.h"
#include "app_settings.h"
#include "app_wifi.h"
#include "app_ota.h"
#include "esp_flash_partitions.h"
#include "esp_partition.h"
#include "esp_ota_ops.h"
#include "errno.h"

#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE

#include "esp_log.h"

#define HASH_LEN 32 /* SHA-256 digest length */

static const char *TAG = "application_main";
EventGroupHandle_t event_group;

static void print_sha256 (const uint8_t *image_hash, const char *label)
{
    char hash_print[HASH_LEN * 2 + 1];
    hash_print[HASH_LEN * 2] = 0;
    for (int i = 0; i < HASH_LEN; ++i) {
        sprintf(&hash_print[i * 2], "%02x", image_hash[i]);
    }
    ESP_LOGI(TAG, "%s: %s", label, hash_print);
}

void app_shutdown()
{
  app_settings_shutdown();

  app_wifi_shutdown();
}

static const char *humanSize(uint64_t bytes)
{
	char *suffix[] = {"B", "KB", "MB", "GB", "TB"};
	char length = sizeof(suffix) / sizeof(suffix[0]);

	int i = 0;
	double dblBytes = bytes;

	if (bytes > 1024) {
		for (i = 0; (bytes / 1024) > 0 && i<length-1; i++, bytes /= 1024)
			dblBytes = bytes / 1024.0;
	}

	static char output[200];
	sprintf(output, "%.02lf %s", dblBytes, suffix[i]);
	return output;
}

void heap_debug_task(void *pvParameter)
{
  while (1)
  {
    uint32_t free_heap_size = 0, min_free_heap_size = 0;
    free_heap_size = esp_get_free_heap_size();
    min_free_heap_size = esp_get_minimum_free_heap_size();
    ESP_LOGI(TAG, "Free Heap Size: %s (%d bytes)", humanSize(free_heap_size),free_heap_size);
    ESP_LOGI(TAG, "Min Free Heap Size: %s (%d bytes)", humanSize(min_free_heap_size),min_free_heap_size);
    vTaskDelay(5000 / portTICK_PERIOD_MS);
  }
}

void app_main()
{
  uint8_t sha_256[HASH_LEN] = { 0 };
  esp_partition_t partition;

  // get sha256 digest for the partition table
  partition.address   = ESP_PARTITION_TABLE_OFFSET;
  partition.size      = ESP_PARTITION_TABLE_MAX_LEN;
  partition.type      = ESP_PARTITION_TYPE_DATA;
  esp_partition_get_sha256(&partition, sha_256);
  print_sha256(sha_256, "SHA-256 for the partition table: ");

  // get sha256 digest for bootloader
  partition.address   = ESP_BOOTLOADER_OFFSET;
  partition.size      = ESP_PARTITION_TABLE_OFFSET;
  partition.type      = ESP_PARTITION_TYPE_APP;
  esp_partition_get_sha256(&partition, sha_256);
  print_sha256(sha_256, "SHA-256 for bootloader: ");

  // get sha256 digest for running partition
  esp_partition_get_sha256(esp_ota_get_running_partition(), sha_256);
  print_sha256(sha_256, "SHA-256 for current firmware: ");

  // If we've gotten this far it's far to assume we are good to go
  const esp_partition_t *running = esp_ota_get_running_partition();
  esp_ota_img_states_t ota_state;

  esp_err_t errRb = esp_ota_mark_app_valid_cancel_rollback();

  ESP_LOGD(TAG,"OTA State is: %d", ota_state);

  if (esp_ota_get_state_partition(running, &ota_state) == ESP_OK)
  {
    if(ota_state == ESP_OTA_IMG_PENDING_VERIFY)
    {
        ESP_LOGI(TAG, "Update was successful, switching to run this going forward");
        esp_err_t errRb = esp_ota_mark_app_valid_cancel_rollback();
    }
    else
    {
        ESP_LOGE(TAG, "Update was not sucessful, rolling back the firmware and rebooting");
        esp_err_t errRb = esp_ota_mark_app_invalid_rollback_and_reboot();
    }
  }

  // Pause so things can be read
  vTaskDelay(10000 / portTICK_PERIOD_MS);

  EventBits_t uxBits;

  // xTaskCreate(&heap_debug_task,"heap_debug_task",2048,NULL,5,NULL);

  ESP_ERROR_CHECK(esp_event_loop_create_default());
  event_group = xEventGroupCreate();

  app_settings_startup();
  // app_settings_reset();
  // app_settings_save();

  app_wifi_startup();

  for (;;)
  {
    uxBits = xEventGroupWaitBits(event_group, WIFI_CONNECTED_BIT | WIFI_SOFTAP_BIT, pdFALSE, pdFALSE, 500 / portTICK_PERIOD_MS);
    if (uxBits > 0)
    {
      ota_init();
      return;
    }
  }

  esp_register_shutdown_handler(&app_shutdown);
}