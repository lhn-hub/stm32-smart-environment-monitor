#ifndef KVSTORE_H
#define KVSTORE_H
#include "app_config.h"
#include <stdint.h>
typedef struct {
    uint32_t magic,version,seq;
    int32_t temp_x10;
    uint32_t distance_cm,smoke_ms,air_ms,intrusion_ms,release_ms,telemetry_ms;
    uint32_t mode,mqtt_port,mqtt_keepalive;
    char ssid[APP_CFG_SSID],wifi_password[APP_CFG_PASSWORD],mqtt_host[APP_CFG_HOST];
    char mqtt_user[APP_CFG_USER],mqtt_password[APP_CFG_PASSWORD],device[APP_CFG_DEVICE];
    uint8_t reserved[APP_CFG_RESERVED];
    uint32_t crc32,commit;
} config_t;
/** @brief Fill RAM defaults, including zero reserved bytes. No Flash operation. */
void config_defaults(config_t *out);
int config_valid(const config_t *config);
/** @brief Scan independent sectors; newest valid CRC+commit wins. LogTask only.
 * @return0 loaded,1 empty initialized defaults,-1 IO/corruption unsupported.
 */
int kv_load(config_t *out);
/** @brief Append alternate Bank, commit last, readback; update out seq on success.
 * On IO uncertainty caller must take storage offline and rescan before retry.
 */
int kv_save(config_t *config);
uint32_t storage_crc32(const void *data,uint32_t bytes);
/** @brief Compare wrapping32-bit sequence numbers, bounded live window. */
int storage_seq_newer(uint32_t a,uint32_t b);
#endif
