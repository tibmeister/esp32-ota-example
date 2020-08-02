#ifndef _APP_OTA_H_
#define _APP_OTA_H_

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t validate_image_header();
void ota_task();
void ota_init();

#ifdef __cplusplus
}
#endif

#endif