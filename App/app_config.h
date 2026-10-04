#ifndef APP_CONFIG_H
#define APP_CONFIG_H
#include "bsp_pin.h"
#define APP_FW_VERSION "0.1.0"
#define APP_WIFI_SSID ""
#define APP_WIFI_PASSWORD ""
#define APP_MQTT_HOST "broker.emqx.io"
#define APP_MQTT_PORT 1883u
#define APP_MQTT_TOPIC_ROOT "smshub/%s"
#define APP_MQTT_TOPIC_TELEMETRY APP_MQTT_TOPIC_ROOT "/telemetry"
#define APP_MQTT_TOPIC_EVENT APP_MQTT_TOPIC_ROOT "/event"
#define APP_MQTT_TOPIC_STATUS APP_MQTT_TOPIC_ROOT "/status"
#define APP_MQTT_TOPIC_CMD APP_MQTT_TOPIC_ROOT "/cmd"
#define APP_MQTT_TOPIC_RESPONSE APP_MQTT_TOPIC_ROOT "/cmd/resp"
#define APP_MQTT_LWT "offline"
#define APP_TELEMETRY_MS DEF_TELEMETRY_PERIOD_MS
#define APP_MQ_WARMUP_MS (MQ_WARMUP_S * 1000u)
/* Stack sizes are StackType_t words (4 bytes on Cortex-M3). */
#define APP_MONITOR_PRIORITY 5u
#define APP_MONITOR_STACK 192u
#define APP_ALARM_PRIORITY 4u
#define APP_ALARM_STACK 256u
#define APP_NET_PRIORITY 3u
#define APP_NET_STACK 512u
#define APP_SENSOR_PRIORITY 2u
#define APP_SENSOR_STACK 256u
#define APP_MENU_PRIORITY 2u
#define APP_MENU_STACK 384u
#define APP_LOG_PRIORITY 1u
#define APP_LOG_STACK 256u
/* Initial scheduling defaults; event-driven tasks block on their queues. */
#define APP_MONITOR_PERIOD_MS 100u
#define APP_SENSOR_PERIOD_MS MQ_SCAN_PERIOD_MS
#define APP_MENU_PERIOD_MS KEY_POLL_MS
#define APP_TASK_WAIT_MS 100u
#define APP_RTOS_HEAP_BYTES (11u * 1024u)
#define APP_RTOS_MAX_SYSCALL_PRIORITY 5u
#define APP_FLASH_BUDGET_BYTES (60u * 1024u)
#define APP_RAM_BUDGET_BYTES (20u * 1024u)
#endif
