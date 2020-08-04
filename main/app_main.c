#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_event.h"
#include "esp_system.h"
#include "esp_log.h"
#include "tcpip_adapter.h"
#include "nvs_flash.h"
#include "sdkconfig.h"
#include "app_settings.h"
#include "app_wifi.h"
#include "app_ota.h"

static const char *TAG = "application_main";
EventGroupHandle_t event_group;

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
  EventBits_t uxBits;

  xTaskCreate(&heap_debug_task,"heap_debug_task",2048,NULL,5,NULL);

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