#ifndef ESP8266_H
#define ESP8266_H

#include <stdint.h>
#include <stddef.h>

typedef enum {
    NETWORK_CMD_ARM = 1,
    NETWORK_CMD_FAN,
    NETWORK_CMD_SERVO,
    NETWORK_CMD_BUZZER,
    NETWORK_CMD_THRESHOLD,
    NETWORK_CMD_REBOOT,
    NETWORK_CMD_ACK,
    NETWORK_CMD_TIME
} network_cmd_kind_t;

typedef struct {
    uint16_t id;
    uint8_t kind;
    uint8_t field;
    int32_t value;
} network_command_t;

typedef struct {
    uint8_t wifi;
    uint8_t mqtt;
    int8_t rssi;
    uint8_t state;
    uint32_t reconnects;
    uint32_t queued;
    uint32_t dropped;
    char ip[16];
    char version[32];
} network_status_t;

enum {
    NETWORK_REPLY_DONE,
    NETWORK_REPLY_QUEUED,
    NETWORK_REPLY_REJECTED,
    NETWORK_REPLY_ACCEPTED
};

void network_init(void);
void network_poll(uint32_t now);
int network_command_pop(network_command_t *command);
void network_command_reply(const network_command_t *command, unsigned status);
int network_reboot_response_sent(void);
int network_event_enqueue(uint32_t type, int32_t parameter, int32_t auxiliary, uint32_t time);
int network_get_status(network_status_t *status);

#endif
