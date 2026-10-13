#include "esp8266.h"
#include "bsp_services.h"
#include "app_config.h"
#include "storage.h"
#include "datahub.h"
#include "sensor.h"
#include "alarm_engine.h"
#include "FreeRTOS.h"
#include "task.h"
#include "stm32f1xx_hal.h"
#include <stdio.h>
#include <string.h>

#define NET_EVENT_RING 20u
#define NET_COMMAND_RING 8u
#define NET_REPLY_RING 8u
#define NET_PAYLOAD_SIZE 96u
#define NET_TOPIC_SIZE 96u
#define NET_CMD_TIMEOUT 3000u
#define NET_JOIN_TIMEOUT 20000u
#define NET_TCP_TIMEOUT 10000u

typedef struct {
    uint16_t type;
    int16_t parameter;
    int16_t auxiliary;
    uint16_t reserved;
} net_event_t;
typedef struct {
    uint16_t id;
    uint8_t kind;
    uint8_t status;
} net_reply_t;
typedef struct {
    uint8_t active;
    uint8_t done;
    int8_t result;
    uint8_t retries;
    uint16_t length;
    const char *expect;
    uint32_t deadline;
} at_request_t;
typedef enum {
    NET_INIT,
    NET_RESET,
    NET_AT,
    NET_GMR,
    NET_MODE,
    NET_JOIN,
    NET_IP,
    NET_RSSI,
    NET_TCP_MUX,
    NET_TCP_MODE,
    NET_TCP_START,
    NET_MQTT_CONNECT,
    NET_MQTT_SUB,
    NET_ONLINE,
    NET_BACKOFF
} net_state_t;
enum { NET_TX_NONE, NET_TX_STATUS, NET_TX_EVENT, NET_TX_REPLY, NET_TX_TELEMETRY, NET_TX_CONFIG };
enum { RAW_TX_NONE, RAW_TX_WAIT_PROMPT, RAW_TX_WAIT_SEND };
enum { MQTT_RX_HEADER, MQTT_RX_REMAINING, MQTT_RX_BODY };

static volatile net_event_t event_ring[NET_EVENT_RING];
static volatile network_command_t command_ring[NET_COMMAND_RING];
static volatile net_reply_t reply_ring[NET_REPLY_RING];
static volatile unsigned event_head, event_tail, event_count, event_dropped;
static volatile unsigned command_head, command_tail;
static volatile unsigned reply_head, reply_tail;
static at_request_t at_request;
static net_state_t state;
static uint8_t state_waiting, state_attempts;
static uint8_t failed_rounds, reset_done;
static uint8_t online_status_pending;
static uint8_t mqtt_conn_ack;
static uint8_t mqtt_conn_wait;
static uint8_t mqtt_sub_ack;
static uint8_t mqtt_sub_wait;
static uint8_t online_tx_kind;
static uint16_t command_id;
static uint32_t backoff_until, next_telemetry, mqtt_conn_deadline;
static uint32_t reconnects;
static network_status_t status;
static uint8_t event_pending;
static net_event_t pending_event;
static uint8_t reply_pending;
static net_reply_t pending_reply;
static uint8_t reboot_reply_sent;
static uint8_t raw_tx_stage;
static uint16_t raw_tx_length;
static uint8_t mqtt_rx_type;
static uint8_t mqtt_rx_stage;
static uint8_t mqtt_rx_flags;
static uint8_t mqtt_rx_rl_bytes;
static uint32_t mqtt_rx_remaining;
static uint32_t mqtt_rx_rl_multiplier;
static uint16_t mqtt_rx_body_pos;
static uint16_t mqtt_rx_topic_length;
static uint16_t mqtt_rx_topic_pos;
static uint16_t mqtt_rx_payload_pos;
static uint8_t mqtt_rx_topic_match;
static char mqtt_cmd_topic[NET_TOPIC_SIZE];
static char mqtt_rx_payload[NET_PAYLOAD_SIZE];
static uint32_t config_sent_seq,config_tx_seq;
static int32_t config_tx_fields[CFG_FIELD_COUNT];
static uint8_t time_request_pending;

static void copy_text(char *dst, size_t capacity, const char *src)
{
    size_t n = strlen(src);
    if (n >= capacity) n = capacity - 1u;
    memcpy(dst, src, n);
    dst[n] = 0;
}

static int ring_event_push(uint32_t type, int32_t parameter, int32_t auxiliary)
{
    unsigned next = (event_head + 1u) % NET_EVENT_RING;
    if (event_count == NET_EVENT_RING) {
        event_tail = (event_tail + 1u) % NET_EVENT_RING;
        ++event_dropped;
        --event_count;
    }
    event_ring[event_head].type = (uint16_t)type;
    event_ring[event_head].parameter = (int16_t)parameter;
    event_ring[event_head].auxiliary = (int16_t)auxiliary;
    event_ring[event_head].reserved = 0;
    event_head = next;
    ++event_count;
    return 0;
}

int network_event_enqueue(uint32_t type, int32_t parameter, int32_t auxiliary, uint32_t time)
{
    (void)time;
    if (!type || __get_IPSR()) return -1;
    taskENTER_CRITICAL();
    ring_event_push(type, parameter, auxiliary);
    taskEXIT_CRITICAL();
    return 0;
}

static int event_pop(net_event_t *event)
{
    int result = -1;
    taskENTER_CRITICAL();
    if (event_count) {
        *event = event_ring[event_tail];
        event_tail = (event_tail + 1u) % NET_EVENT_RING;
        --event_count;
        result = 0;
    }
    taskEXIT_CRITICAL();
    return result;
}

static int command_push(uint8_t kind, uint8_t field, int32_t value)
{
    unsigned next = (command_head + 1u) % NET_COMMAND_RING;
    if (next == command_tail) return -1;
    command_ring[command_head].id = ++command_id;
    command_ring[command_head].kind = kind;
    command_ring[command_head].field = field;
    command_ring[command_head].value = value;
    command_head = next;
    return 0;
}

int network_command_pop(network_command_t *command)
{
    int result = -1;
    if (!command) return -1;
    taskENTER_CRITICAL();
    if (command_tail != command_head) {
        *command = command_ring[command_tail];
        command_tail = (command_tail + 1u) % NET_COMMAND_RING;
        result = 0;
    }
    taskEXIT_CRITICAL();
    return result;
}

void network_command_reply(const network_command_t *command, unsigned reply)
{
    unsigned next;
    if (!command) return;
    taskENTER_CRITICAL();
    next = (reply_head + 1u) % NET_REPLY_RING;
    if (next != reply_tail) {
        reply_ring[reply_head].id = command->id;
        reply_ring[reply_head].kind = command->kind;
        reply_ring[reply_head].status = (uint8_t)reply;
        reply_head = next;
    } else ++status.dropped;
    taskEXIT_CRITICAL();
}

static void reject_downlink(void)
{
    unsigned next;
    taskENTER_CRITICAL();
    next = (reply_head + 1u) % NET_REPLY_RING;
    if (next != reply_tail) {
        reply_ring[reply_head].id = ++command_id;
        reply_ring[reply_head].kind = 0;
        reply_ring[reply_head].status = NETWORK_REPLY_REJECTED;
        reply_head = next;
    } else ++status.dropped;
    taskEXIT_CRITICAL();
}

static int reply_pop(net_reply_t *reply)
{
    int result = -1;
    taskENTER_CRITICAL();
    if (reply_tail != reply_head) {
        *reply = reply_ring[reply_tail];
        reply_tail = (reply_tail + 1u) % NET_REPLY_RING;
        result = 0;
    }
    taskEXIT_CRITICAL();
    return result;
}

int network_reboot_response_sent(void)
{
    return reboot_reply_sent;
}

static const char *command_name(unsigned kind)
{
    switch (kind) {
    case NETWORK_CMD_ARM: return "arm";
    case NETWORK_CMD_FAN: return "fan";
    case NETWORK_CMD_SERVO: return "servo";
    case NETWORK_CMD_BUZZER: return "buzzer";
    case NETWORK_CMD_THRESHOLD: return "th";
    case NETWORK_CMD_REBOOT: return "reboot";
    case NETWORK_CMD_ACK: return "ack";
    case NETWORK_CMD_TIME: return "time";
    default: return "unknown";
    }
}

static int parse_number(const char *text, int32_t *value)
{
    int32_t n = 0;
    int sign = 1;
    if (!text || !*text || !value) return -1;
    if (*text == '-') { sign = -1; ++text; }
    if (!*text) return -1;
    while (*text >= '0' && *text <= '9') {
        if (n > (INT32_MAX - (*text - '0')) / 10) return -1;
        n = n * 10 + (*text++ - '0');
    }
    if (*text) return -1;
    *value = sign < 0 ? -n : n;
    return 0;
}

static int parse_command(const char *payload)
{
    char key[12], value[48];
    const char *equal;
    int32_t number;
    unsigned i = 0;
    if (!payload) return -1;
    equal = strchr(payload, '=');
    if (!equal) equal = strchr(payload, ':');
    if (!equal || equal == payload) return -1;
    if((size_t)(equal-payload)>=sizeof(key) || strlen(equal+1)>=sizeof(value))return -1;
    while (payload[i] && payload + i < equal && i + 1u < sizeof(key)) {
        key[i] = payload[i];
        ++i;
    }
    key[i] = 0;
    copy_text(value, sizeof(value), equal + 1);
    {
        size_t n = strlen(value);
        if (n >= 2u && value[0] == '"' && value[n - 1u] == '"') {
        memmove(value, value + 1, n - 2u);
        value[n - 2u] = 0;
        }
    }
    if (!strcmp(key, "arm")) {
        if (parse_number(value, &number) || (number != 0 && number != 1)) return -1;
        return command_push(NETWORK_CMD_ARM, CFG_MODE, number);
    }
    if (!strcmp(key, "fan")) {
        if (parse_number(value, &number) || number < 0 || number > 100) return -1;
        return command_push(NETWORK_CMD_FAN, 0, number);
    }
    if (!strcmp(key, "servo")) {
        if (parse_number(value, &number) || number < 0 || number > 180) return -1;
        return command_push(NETWORK_CMD_SERVO, 0, number);
    }
    if (!strcmp(key, "buzzer")) {
        if (parse_number(value, &number) || number <= 0 ||
            number > (int32_t)APP_BEEP_MAX_MS) return -1;
        return command_push(NETWORK_CMD_BUZZER, 0, number);
    }
    if (!strcmp(key, "reboot")) {
        if (parse_number(value, &number) || number != 1) return -1;
        return command_push(NETWORK_CMD_REBOOT, 0, number);
    }
    if (!strcmp(key,"ack")) {
        if(parse_number(value,&number) || number!=1)return -1;
        return command_push(NETWORK_CMD_ACK,0,number);
    }
    if (!strcmp(key,"time")) {
        if(parse_number(value,&number) || number<1577836800)return -1;
        return command_push(NETWORK_CMD_TIME,0,number);
    }
    if (!strcmp(key, "th")) {
        char *colon = strchr(value, ':');
        uint8_t field = CFG_TEMP;
        if (colon) {
            *colon++ = 0;
            if (!strcmp(value, "temp")) field = CFG_TEMP;
            else if (!strcmp(value, "distance")) field = CFG_DISTANCE;
            else if (!strcmp(value, "smoke")) field = CFG_SMOKE;
            else if (!strcmp(value, "air")) field = CFG_AIR;
            else return -1;
            value[0] = 0;
            copy_text(value, sizeof(value), colon);
        }
        if (parse_number(value, &number) || number<0 ||
            (field==CFG_TEMP && number>APP_EDIT_MAX_TEMP) ||
            (field==CFG_DISTANCE && number>APP_EDIT_MAX_DISTANCE) ||
            ((field==CFG_SMOKE || field==CFG_AIR) && number>APP_EDIT_MAX_CONFIRM_MS)) return -1;
        return command_push(NETWORK_CMD_THRESHOLD, field, number);
    }
    return -1;
}

/* Legacy line-mode MQTT URCs are intentionally unsupported by the raw TCP path. */
void net_on_rx_line(const char *line, size_t length)
{
    (void)length;
    if (!line) return;
    if ((state == NET_AT || state == NET_GMR) && line[0])
        LOGI("ESP启动应答：%s",line);
    if (at_request.active && at_request.expect && strstr(line, at_request.expect)) {
        at_request.result = 1;
        at_request.active = 0;
        at_request.done = 1;
    } else if (at_request.active && state == NET_TCP_START &&
               !strcmp(line, "CONNECT")) {
        at_request.result = 1;
        at_request.active = 0;
        at_request.done = 1;
    } else if (at_request.active &&
               (strstr(line, "ERROR") || strstr(line, "FAIL"))) {
        at_request.result = -1;
        at_request.active = 0;
        at_request.done = 1;
    }
    if (strstr(line, "WIFI CONNECTED") || strstr(line, "WIFI GOT IP")) status.wifi = 1;
    if (strstr(line, "WIFI DISCONNECTED")) { status.wifi = 0; status.mqtt = 0; }
    if (!strcmp(line, "CLOSED")) status.mqtt = 0;
    if (!strncmp(line, "+CIFSR:STAIP,", 13)) {
        const char *q = strchr(line, '"');
        if (q) {
            const char *e = strchr(q + 1, '"');
            if (e) {
                size_t n = (size_t)(e - q - 1);
                if (n >= sizeof(status.ip)) n = sizeof(status.ip) - 1u;
                memcpy(status.ip, q + 1, n);
                status.ip[n] = 0;
            }
        }
    }
    if (!strncmp(line, "+CWJAP:", 7)) {
        const char *comma = strrchr(line, ',');
        int32_t rssi;
        if (comma && !parse_number(comma + 1, &rssi)) status.rssi = (int8_t)rssi;
    }
    if (line[0] && line[0] != '+' && strcmp(line, "OK") && strcmp(line, "ERROR") &&
        strcmp(line, "SEND OK") && strcmp(line, "SEND FAIL") &&
        strcmp(line, "CONNECT") && strcmp(line, "CLOSED") &&
        strcmp(line, "ready") && strncmp(line, "WIFI ", 5))
    {
        copy_text(status.version, sizeof(status.version), line);
    }
}

static void mqtt_rx_reset(void)
{
    mqtt_rx_stage = MQTT_RX_HEADER;
    mqtt_rx_type = 0;
    mqtt_rx_flags = 0;
    mqtt_rx_rl_bytes = 0;
    mqtt_rx_remaining = 0;
    mqtt_rx_rl_multiplier = 1;
    mqtt_rx_body_pos = 0;
    mqtt_rx_topic_length = 0;
    mqtt_rx_topic_pos = 0;
    mqtt_rx_payload_pos = 0;
    mqtt_rx_topic_match = 1;
}

static void mqtt_rx_finish(void)
{
    if (mqtt_rx_type == 2u && mqtt_rx_payload_pos >= 2u) {
        if (mqtt_rx_payload[1] == 0u) mqtt_conn_ack = 1;
    } else if (mqtt_rx_type == 9u && mqtt_rx_payload_pos >= 3u) {
        if ((uint8_t)mqtt_rx_payload[2] <= 1u) mqtt_sub_ack = 1;
    } else if (mqtt_rx_type == 3u && mqtt_rx_topic_match &&
               mqtt_rx_payload_pos < sizeof(mqtt_rx_payload)) {
        mqtt_rx_payload[mqtt_rx_payload_pos] = 0;
        if (mqtt_rx_topic_length == strlen(mqtt_cmd_topic))
            if (parse_command(mqtt_rx_payload)) {
                ++status.dropped;
                reject_downlink();
            }
    }
    mqtt_rx_reset();
}

void net_on_rx_bytes(const uint8_t *data, size_t length)
{
    size_t i;
    if (!data) return;
    for (i = 0; i < length; ++i) {
        uint8_t byte = data[i];
        if (mqtt_rx_stage == MQTT_RX_HEADER) {
            mqtt_rx_type = byte >> 4;
            mqtt_rx_flags = byte & 0x0fu;
            mqtt_rx_rl_bytes = 0;
            mqtt_rx_remaining = 0;
            mqtt_rx_rl_multiplier = 1;
            mqtt_rx_stage = MQTT_RX_REMAINING;
            continue;
        }
        if (mqtt_rx_stage == MQTT_RX_REMAINING) {
            if (mqtt_rx_rl_bytes >= 4u) { mqtt_rx_reset(); continue; }
            mqtt_rx_remaining += (uint32_t)(byte & 0x7fu) * mqtt_rx_rl_multiplier;
            ++mqtt_rx_rl_bytes;
            if (mqtt_rx_remaining > NET_PAYLOAD_SIZE + NET_TOPIC_SIZE + 8u) {
                mqtt_rx_reset();
                continue;
            }
            if (byte & 0x80u) {
                mqtt_rx_rl_multiplier *= 128u;
            } else {
                mqtt_rx_body_pos = 0;
                mqtt_rx_payload_pos = 0;
                mqtt_rx_topic_length = 0;
                mqtt_rx_topic_pos = 0;
            mqtt_rx_topic_match = (mqtt_rx_type != 3u || (mqtt_rx_flags & 0x06u) == 0u);
                mqtt_rx_stage = MQTT_RX_BODY;
                if (!mqtt_rx_remaining) mqtt_rx_finish();
            }
            continue;
        }
        if (mqtt_rx_type == 2u) {
            if (mqtt_rx_payload_pos < sizeof(mqtt_rx_payload))
                mqtt_rx_payload[mqtt_rx_payload_pos] = (char)byte;
            ++mqtt_rx_payload_pos;
        } else if (mqtt_rx_type == 9u) {
            if (mqtt_rx_payload_pos < sizeof(mqtt_rx_payload))
                mqtt_rx_payload[mqtt_rx_payload_pos] = (char)byte;
            mqtt_rx_payload_pos++;
        } else if (mqtt_rx_type == 3u) {
            if (mqtt_rx_body_pos == 0u) {
                mqtt_rx_topic_length = (uint16_t)byte << 8;
            } else if (mqtt_rx_body_pos == 1u) {
                mqtt_rx_topic_length |= byte;
            } else if (mqtt_rx_body_pos < 2u + mqtt_rx_topic_length) {
                mqtt_rx_topic_pos = (uint16_t)(mqtt_rx_body_pos - 2u);
                if (mqtt_rx_topic_pos >= strlen(mqtt_cmd_topic) ||
                    byte != (uint8_t)mqtt_cmd_topic[mqtt_rx_topic_pos])
                    mqtt_rx_topic_match = 0;
            } else {
                if (mqtt_rx_payload_pos + 1u < sizeof(mqtt_rx_payload))
                    mqtt_rx_payload[mqtt_rx_payload_pos] = (char)byte;
                ++mqtt_rx_payload_pos;
            }
            ++mqtt_rx_body_pos;
        }
        if (mqtt_rx_remaining) --mqtt_rx_remaining;
        if (!mqtt_rx_remaining) mqtt_rx_finish();
    }
}

void net_on_rx_prompt(void)
{
    if (at_request.active && at_request.expect && !strcmp(at_request.expect, ">")) {
        at_request.result = 1;
        at_request.active = 0;
        at_request.done = 1;
    }
}

static int at_send_cmd(const char *command, const char *expect, uint32_t timeout, uint8_t retry)
{
    size_t n;
    if (!command || !expect || at_request.active || !retry) return -1;
    n = strlen(command);
    if (bsp_net_send(command, n)) return -1;
    at_request.active = 1;
    at_request.done = 0;
    at_request.result = 0;
    at_request.retries = retry;
    at_request.length = (uint16_t)n;
    at_request.expect = expect;
    at_request.deadline = HAL_GetTick() + timeout;
    return 0;
}

static void at_poll(uint32_t now)
{
    if (!at_request.active) return;
    if ((int32_t)(now - at_request.deadline) < 0) return;
    LOGW("ESP AT等待超时 状态=%u",(unsigned)state);
    if (at_request.retries > 1u) {
        --at_request.retries;
        if (bsp_net_send_buffer(at_request.length)) {
            at_request.active = 0;
            at_request.done = 1;
            at_request.result = -1;
        } else at_request.deadline = now + NET_CMD_TIMEOUT;
    } else {
        at_request.active = 0;
        at_request.done = 1;
        at_request.result = -1;
    }
}

static int at_take(void)
{
    int result;
    if (!at_request.done) return 0;
    result = at_request.result;
    at_request.done = 0;
    return result > 0 ? 1 : -1;
}

static void round_failed(uint32_t now);

static void enter_reset(void)
{
    state = NET_RESET;
    state_attempts = 0;
    state_waiting = 0;
    reset_done = 0;
    mqtt_conn_wait = 0;
    mqtt_conn_ack = 0;
    mqtt_sub_wait = 0;
    mqtt_sub_ack = 0;
    raw_tx_stage = RAW_TX_NONE;
    online_tx_kind = NET_TX_NONE;
    status.mqtt = 0;
    at_request.active = 0;
    at_request.done = 0;
    mqtt_rx_reset();
}

static void state_retry_or_reset(uint32_t now)
{
    if (++state_attempts < ESP_RETRY_MAX) return;
    round_failed(now);
}

static void round_failed(uint32_t now)
{
    status.wifi = status.mqtt = 0;
    ++reconnects;
    if (++failed_rounds >= 5u) {
        failed_rounds = 0;
        backoff_until = now + ESP_BACKOFF_MS;
        state = NET_BACKOFF;
    } else state = NET_RESET;
    state_waiting = 0;
    state_attempts = 0;
    reset_done = 0;
    online_status_pending = 0;
    mqtt_conn_wait = 0;
    mqtt_conn_ack = 0;
    at_request.active = 0;
    at_request.done = 0;
    mqtt_rx_reset();
}

static int config_ready(void)
{
    char ssid[APP_CFG_SSID], host[APP_CFG_HOST];
    if (storage_get_network_text(STORAGE_NET_SSID, ssid, sizeof(ssid)) ||
        storage_get_network_text(STORAGE_NET_HOST, host, sizeof(host))) return 0;
    return ssid[0] && host[0];
}

static int build_join(void)
{
    char ssid[APP_CFG_SSID], password[APP_CFG_PASSWORD];
    char *tx;
    size_t cap;
    if (storage_get_network_text(STORAGE_NET_SSID, ssid, sizeof(ssid)) ||
        storage_get_network_text(STORAGE_NET_PASSWORD, password, sizeof(password))) return -1;
    tx = bsp_net_tx_buffer(&cap);
    return (int)snprintf(tx, cap, "AT+CWJAP=\"%s\",\"%s\"\r\n", ssid, password);
}

static int build_topic(char *topic, size_t capacity, const char *suffix)
{
    char device[APP_CFG_DEVICE];
    int n;
    if (storage_get_network_text(STORAGE_NET_DEVICE, device, sizeof(device))) return -1;
    n = snprintf(topic, capacity, "smshub/%s/%s", device, suffix);
    return n < 0 || (size_t)n >= capacity ? -1 : 0;
}

static uint8_t *mqtt_put_string(uint8_t *p, const uint8_t *end, const char *text)
{
    size_t n = text ? strlen(text) : 0u;
    if (n > 65535u || p + 2u + n > end) return NULL;
    *p++ = (uint8_t)(n >> 8);
    *p++ = (uint8_t)n;
    memcpy(p, text, n);
    return p + n;
}

static int mqtt_finish_packet(uint8_t header, uint8_t *body_end)
{
    uint8_t *tx;
    size_t cap, body_len, total, n = 0;
    uint32_t remaining;
    tx = (uint8_t *)bsp_net_tx_buffer(&cap);
    body_len = (size_t)(body_end - (tx + 5u));
    remaining = (uint32_t)body_len;
    do { ++n; remaining >>= 7; } while (remaining);
    total = 1u + n + body_len;
    if (total > cap) return -1;
    memmove(tx + 1u + n, tx + 5u, body_len);
    tx[0] = header;
    remaining = (uint32_t)body_len;
    for (size_t i = 0; i < n; ++i) {
        uint8_t byte = (uint8_t)(remaining & 0x7fu);
        remaining >>= 7;
        if (remaining) byte |= 0x80u;
        tx[1u + i] = byte;
    }
    return (int)total;
}

static int mqtt_build_connect(void)
{
    char user[APP_CFG_USER], password[APP_CFG_PASSWORD];
    char client[24], topic[NET_TOPIC_SIZE];
    uint32_t uid = 0, port, keepalive;
    uint8_t *tx, *p, *end;
    size_t cap;
    uint8_t flags = 0x26u;
#ifdef STM32F103xB
    uid = *(const volatile uint32_t *)0x1FFFF7E8u;
#endif
    if (storage_get_network_text(STORAGE_NET_USER, user, sizeof(user)) ||
        storage_get_network_text(STORAGE_NET_MQTT_PASSWORD, password, sizeof(password)) ||
        storage_get_network_numbers(&port, &keepalive) ||
        build_topic(topic, sizeof(topic), "status")) return -1;
    (void)port;
    if (keepalive > 65535u) keepalive = APP_MQTT_KEEPALIVE;
    if (snprintf(client, sizeof(client), "sms01%08lX", (unsigned long)uid) >= (int)sizeof(client))
        return -1;
    if (user[0] || password[0]) flags |= 0x80u;
    if (password[0]) flags |= 0x40u;
    tx = (uint8_t *)bsp_net_tx_buffer(&cap);
    p = tx + 5u; end = tx + cap;
    p = mqtt_put_string(p, end, "MQTT");
    if (!p || p + 4u > end) return -1;
    *p++ = 4u; *p++ = flags;
    *p++ = (uint8_t)(keepalive >> 8);
    *p++ = (uint8_t)keepalive;
    p = mqtt_put_string(p, end, client);
    if (!p) return -1;
    p = mqtt_put_string(p, end, topic);
    if (!p) return -1;
    p = mqtt_put_string(p, end, APP_MQTT_LWT);
    if (!p) return -1;
    if (user[0]) p = mqtt_put_string(p, end, user);
    if (!p) return -1;
    if (password[0]) p = mqtt_put_string(p, end, password);
    return p ? mqtt_finish_packet(0x10u, p) : -1;
}

static int mqtt_build_subscribe(void)
{
    uint8_t *tx, *p, *end;
    size_t cap;
    if (build_topic(mqtt_cmd_topic, sizeof(mqtt_cmd_topic), "cmd")) return -1;
    tx = (uint8_t *)bsp_net_tx_buffer(&cap);
    p = tx + 5u; end = tx + cap;
    if (p + 2u > end) return -1;
    *p++ = 0u; *p++ = 1u;
    p = mqtt_put_string(p, end, mqtt_cmd_topic);
    if (!p || p + 1u > end) return -1;
    *p++ = 0u;
    return mqtt_finish_packet(0x82u, p);
}

static int mqtt_build_publish(const char *topic, const char *payload, unsigned retain)
{
    uint8_t *tx, *p, *end;
    size_t cap;
    tx = (uint8_t *)bsp_net_tx_buffer(&cap);
    p = tx + 5u; end = tx + cap;
    p = mqtt_put_string(p, end, topic);
    if (!p) return -1;
    if (p + strlen(payload) > end) return -1;
    memcpy(p, payload, strlen(payload)); p += strlen(payload);
    return mqtt_finish_packet((uint8_t)(0x30u | (retain ? 1u : 0u)), p);
}

enum { MQTT_PKT_CONNECT=1, MQTT_PKT_SUBSCRIBE, MQTT_PKT_STATUS,
       MQTT_PKT_EVENT, MQTT_PKT_REPLY, MQTT_PKT_TELEMETRY, MQTT_PKT_CONFIG };
static uint8_t raw_packet_kind;

static int mqtt_build_raw_packet(uint8_t kind)
{
    char topic[NET_TOPIC_SIZE], payload[190];
    if (kind == MQTT_PKT_CONNECT) return mqtt_build_connect();
    if (kind == MQTT_PKT_SUBSCRIBE) return mqtt_build_subscribe();
    if (kind == MQTT_PKT_STATUS) {
        if (build_topic(topic, sizeof(topic), "status")) return -1;
        return mqtt_build_publish(topic, "online", 1u);
    }
    if(kind==MQTT_PKT_CONFIG){
        if(build_topic(topic,sizeof(topic),"config"))return -1;
        snprintf(payload,sizeof(payload),
                 "{\"seq\":%lu,\"arm\":%ld,\"temp\":%ld,\"distance\":%ld,\"smoke\":%ld,\"air\":%ld}",
                 (unsigned long)config_tx_seq,(long)config_tx_fields[CFG_MODE],(long)config_tx_fields[CFG_TEMP],
                 (long)config_tx_fields[CFG_DISTANCE],(long)config_tx_fields[CFG_SMOKE],(long)config_tx_fields[CFG_AIR]);
        return mqtt_build_publish(topic,payload,1u);
    }
    if (kind == MQTT_PKT_EVENT) {
        if (build_topic(topic, sizeof(topic), "event")) return -1;
        snprintf(payload, sizeof(payload), "{\"type\":%u,\"p\":%d,\"a\":%d}",
                 (unsigned)pending_event.type, (int)pending_event.parameter,
                 (int)pending_event.auxiliary);
        return mqtt_build_publish(topic, payload, 0u);
    }
    if (kind == MQTT_PKT_REPLY) {
        static const char *status_text[] = {"done", "queued", "rejected", "accepted"};
        if (build_topic(topic, sizeof(topic), "cmd/resp")) return -1;
        snprintf(payload, sizeof(payload),
                 "{\"id\":%u,\"cmd\":\"%s\",\"status\":\"%s\"}",
                 (unsigned)pending_reply.id, command_name(pending_reply.kind),
                 pending_reply.status < 4u ? status_text[pending_reply.status] : "rejected");
        return mqtt_build_publish(topic, payload, 0u);
    }
    {
        datahub_snapshot_t snapshot;
        const sensor_value_t *t, *u, *r, *s, *a;
        network_status_t net;
        if (datahub_snapshot(&snapshot) || build_topic(topic, sizeof(topic), "telemetry"))
            return -1;
        t=&snapshot.sensors[SENSOR_DHT11]; u=&snapshot.sensors[SENSOR_DISTANCE];
        r=&snapshot.sensors[SENSOR_RADAR]; s=&snapshot.sensors[SENSOR_SMOKE];
        a=&snapshot.sensors[SENSOR_AIR]; network_get_status(&net);
        snprintf(payload, sizeof(payload),
                 "{\"ts\":%lu,\"temp\":%ld,\"hum\":%ld,\"distance\":%ld,\"human\":%ld,\"smoke\":%ld,\"air\":%ld,\"wifi\":%u,\"mqtt\":%u}",
                 (unsigned long)rtc_get(), (long)t->value, (long)t->auxiliary,
                 (long)u->value, (long)r->value, (long)s->value, (long)a->value,
                 (unsigned)net.wifi, (unsigned)net.mqtt);
        return mqtt_build_publish(topic, payload, 0u);
    }
}

static int mqtt_raw_begin(uint8_t kind, uint32_t now)
{
    char *tx;
    size_t cap;
    int length = mqtt_build_raw_packet(kind);
    (void)now;
    if (length < 0) return -1;
    raw_packet_kind = kind;
    raw_tx_length = (uint16_t)length;
    tx = bsp_net_tx_buffer(&cap);
    if (snprintf(tx, cap, "AT+CIPSEND=%u\r\n", (unsigned)raw_tx_length) >= (int)cap ||
        at_send_cmd(tx, ">", NET_TCP_TIMEOUT, 1u)) return -1;
    raw_tx_stage = RAW_TX_WAIT_PROMPT;
    return 0;
}

static int mqtt_raw_poll(uint32_t now)
{
    int result;
    if (!raw_tx_stage) return 0;
    result = at_take();
    if (!result) return 1;
    if (result < 0) { raw_tx_stage = RAW_TX_NONE; return -1; }
    if (raw_tx_stage == RAW_TX_WAIT_PROMPT) {
        if (mqtt_build_raw_packet(raw_packet_kind) != (int)raw_tx_length ||
            bsp_net_send_buffer(raw_tx_length)) {
            raw_tx_stage = RAW_TX_NONE; return -1;
        }
        at_request.active = 1; at_request.done = 0; at_request.result = 0;
        at_request.retries = 1; at_request.length = 0;
        at_request.expect = "SEND OK"; at_request.deadline = now + NET_TCP_TIMEOUT;
        raw_tx_stage = RAW_TX_WAIT_SEND;
        return 1;
    }
    raw_tx_stage = RAW_TX_NONE;
    if (raw_packet_kind == MQTT_PKT_CONNECT) {
        mqtt_conn_wait = 1; mqtt_conn_deadline = now + NET_CMD_TIMEOUT;
    } else if (raw_packet_kind == MQTT_PKT_SUBSCRIBE) {
        mqtt_sub_wait = 1; mqtt_conn_deadline = now + NET_CMD_TIMEOUT;
    } else {
        if (online_tx_kind == NET_TX_STATUS) online_status_pending = 0;
        if (online_tx_kind == NET_TX_CONFIG) config_sent_seq=config_tx_seq;
        if (online_tx_kind == NET_TX_EVENT) event_pending = 0;
        if (online_tx_kind == NET_TX_REPLY) {
            reply_pending = 0;
            if (pending_reply.kind == NETWORK_CMD_REBOOT) reboot_reply_sent = 1;
        }
        online_tx_kind = NET_TX_NONE;
    }
    return 1;
}

static int publish_event_start(void)
{
    net_event_t event;
    if (!event_pending) {
        if (event_pop(&event)) return 0;
        pending_event = event; event_pending = 1;
    }
    return mqtt_raw_begin(MQTT_PKT_EVENT, HAL_GetTick()) ? -1 : 1;
}

static int publish_reply_start(void)
{
    net_reply_t reply;
    if (!reply_pending) {
        if (reply_pop(&reply)) return 0;
        pending_reply = reply; reply_pending = 1;
    }
    return mqtt_raw_begin(MQTT_PKT_REPLY, HAL_GetTick()) ? -1 : 1;
}

static void online_poll(uint32_t now)
{
    int result;
    if (!status.mqtt) { enter_reset(); return; }
    if (online_status_pending) {
        if (mqtt_raw_begin(MQTT_PKT_STATUS, now)) { enter_reset(); return; }
        online_tx_kind = NET_TX_STATUS; return;
    }
    if(time_request_pending && !event_count && !event_pending){
        network_event_enqueue(EVENT_TIME_REQUEST,0,0,rtc_get());time_request_pending=0;
    }
    if (event_count || event_pending) {
        result = publish_event_start();
        if (result < 0) { enter_reset(); return; }
        if (result > 0) { online_tx_kind = NET_TX_EVENT; return; }
    }
    if (reply_tail != reply_head || reply_pending) {
        result = publish_reply_start();
        if (result < 0) { enter_reset(); return; }
        if (result > 0) { online_tx_kind = NET_TX_REPLY; return; }
    }
    {
        storage_status_t store;
        if(!storage_get_fields(config_tx_fields,&store) && store.ready && store.config_seq!=config_sent_seq){
            config_tx_seq=store.config_seq;
            if(mqtt_raw_begin(MQTT_PKT_CONFIG,now)){enter_reset();return;}
            online_tx_kind=NET_TX_CONFIG;return;
        }
    }
    if ((int32_t)(now - next_telemetry) >= 0) {
        if (mqtt_raw_begin(MQTT_PKT_TELEMETRY, now)) { enter_reset(); return; }
        next_telemetry = now + APP_TELEMETRY_MS;
        online_tx_kind = NET_TX_TELEMETRY;
    }
}

void network_init(void)
{
    memset((void *)&status, 0, sizeof(status));
    status.rssi = -127;
    config_sent_seq=0;
    time_request_pending=0;
    state = NET_INIT;
    state_attempts = 0;
    state_waiting = 0;
    reset_done = 0;
    online_status_pending = 0;
    event_pending = 0;
    mqtt_conn_ack = 0;
    mqtt_sub_ack = 0;
    mqtt_conn_wait = 0;
    mqtt_sub_wait = 0;
    raw_tx_stage = RAW_TX_NONE;
    raw_packet_kind = 0;
    mqtt_rx_reset();
    event_head = event_tail = event_count = event_dropped = 0;
    command_head = command_tail = 0;
    reply_head = reply_tail = 0;
    event_pending = 0;
    reply_pending = 0;
    reboot_reply_sent = 0;
    reconnects = 0;
    failed_rounds = 0;
    memset((void *)&status.ip, 0, sizeof(status.ip));
    next_telemetry = HAL_GetTick() + APP_TELEMETRY_MS;
}

void network_poll(uint32_t now)
{
    int result;
    char host[APP_CFG_HOST], *tx;
    size_t cap;
    uint32_t port, keepalive;
    at_poll(now);
    result = mqtt_raw_poll(now);
    if (result < 0) {
        if (state == NET_ONLINE) enter_reset();
        else state_retry_or_reset(now);
        return;
    }
    if (result > 0) return;
    if (state == NET_BACKOFF) {
        if ((int32_t)(now - backoff_until) >= 0) {
            state = NET_RESET;
            reset_done = 0;
        }
        else return;
    }
    if (mqtt_conn_wait) {
        if (mqtt_conn_ack) {
            mqtt_conn_wait = 0;
            state = NET_MQTT_SUB;
            status.mqtt = 1;
        } else if ((int32_t)(now - mqtt_conn_deadline) >= 0) {
            mqtt_conn_wait = 0;
            state_retry_or_reset(now);
        }
        return;
    }
    if (mqtt_sub_wait) {
        if (mqtt_sub_ack) {
            mqtt_sub_wait = 0;
            mqtt_sub_ack = 0;
            failed_rounds = 0; online_status_pending = 1;
            state = NET_ONLINE;
            config_sent_seq=0;
            time_request_pending=1;
            next_telemetry = now;
            LOGI("网络在线 IP=%s RSSI=%d",status.ip,status.rssi);
        } else if ((int32_t)(now - mqtt_conn_deadline) >= 0) {
            mqtt_sub_wait = 0;
            state_retry_or_reset(now);
        }
        return;
    }
    if (state == NET_ONLINE) { online_poll(now); return; }
    if (state_waiting) {
        result = at_take();
        if (!result) return;
        state_waiting = 0;
        if (result < 0) {
            state_retry_or_reset(now);
            return;
        }
        state_attempts = 0;
        switch (state) {
        case NET_AT: alarm_selftest_report(FAULT_ESP,1);state = NET_GMR; break;
        case NET_GMR: state = NET_MODE; break;
        case NET_MODE: state = NET_JOIN; break;
        case NET_JOIN: state = NET_IP; break;
        case NET_IP: state = NET_RSSI; break;
        case NET_RSSI: state = NET_TCP_MUX; break;
        case NET_TCP_MUX: state = NET_TCP_MODE; break;
        case NET_TCP_MODE: state = NET_TCP_START; break;
        case NET_TCP_START: state = NET_MQTT_CONNECT; break;
        default: break;
        }
        return;
    }
    if (state == NET_INIT) {
        if (!config_ready()) return;
        state = NET_RESET; state_attempts = 0; reset_done = 0;
        return;
    }
    if (state == NET_RESET) {
        if (!reset_done) {
            if (bsp_net_hard_reset()) { round_failed(now); return; }
            reset_done = 1; mqtt_conn_ack = 0; state = NET_AT; state_attempts = 0;
        }
        return;
    }
    if (state == NET_AT) {
        if (at_send_cmd("AT\r\n","OK",NET_CMD_TIMEOUT,1)) state_retry_or_reset(now);
        else state_waiting = 1;
    } else if (state == NET_GMR) {
        if (at_send_cmd("AT+GMR\r\n","OK",NET_CMD_TIMEOUT,1)) state_retry_or_reset(now);
        else state_waiting = 1;
    } else if (state == NET_MODE) {
        if (at_send_cmd("AT+CWMODE=1\r\n","OK",NET_CMD_TIMEOUT,1)) state_retry_or_reset(now);
        else state_waiting = 1;
    } else if (state == NET_JOIN) {
        int n = build_join();
        char *tx = bsp_net_tx_buffer(NULL);
        if (n < 0 || (size_t)n >= APP_NET_TX_SIZE ||
            at_send_cmd(tx,"OK",NET_JOIN_TIMEOUT,1)) state_retry_or_reset(now);
        else state_waiting = 1;
    } else if (state == NET_IP) {
        status.ip[0] = 0;
        if (at_send_cmd("AT+CIFSR\r\n","OK",NET_CMD_TIMEOUT,1)) state_retry_or_reset(now);
        else state_waiting = 1;
    } else if (state == NET_RSSI) {
        if (at_send_cmd("AT+CWJAP?\r\n","OK",NET_CMD_TIMEOUT,1)) state_retry_or_reset(now);
        else state_waiting = 1;
    } else if (state == NET_TCP_MUX) {
        if (at_send_cmd("AT+CIPMUX=0\r\n","OK",NET_CMD_TIMEOUT,1)) state_retry_or_reset(now);
        else state_waiting = 1;
    } else if (state == NET_TCP_MODE) {
        if (at_send_cmd("AT+CIPMODE=0\r\n","OK",NET_CMD_TIMEOUT,1)) state_retry_or_reset(now);
        else state_waiting = 1;
    } else if (state == NET_TCP_START) {
        if (storage_get_network_text(STORAGE_NET_HOST, host, sizeof(host)) ||
            storage_get_network_numbers(&port, &keepalive)) {
            state_retry_or_reset(now);
            return;
        }
        (void)keepalive;
        tx = bsp_net_tx_buffer(&cap);
        if (snprintf(tx, cap, "AT+CIPSTART=\"TCP\",\"%s\",%lu\r\n",
                     host, (unsigned long)port) >= (int)cap ||
            at_send_cmd(tx,"OK",NET_TCP_TIMEOUT,1)) state_retry_or_reset(now);
        else state_waiting = 1;
    } else if (state == NET_MQTT_CONNECT) {
        if (mqtt_raw_begin(MQTT_PKT_CONNECT, now)) state_retry_or_reset(now);
    } else if (state == NET_MQTT_SUB) {
        mqtt_sub_ack = 0;
        if (mqtt_raw_begin(MQTT_PKT_SUBSCRIBE, now)) state_retry_or_reset(now);
    }
}

int network_get_status(network_status_t *out)
{
    if (!out) return -1;
    taskENTER_CRITICAL();
    *out = status;
    out->reconnects = reconnects;
    out->queued = (command_head + NET_COMMAND_RING - command_tail) % NET_COMMAND_RING;
    out->dropped = event_dropped;
    taskEXIT_CRITICAL();
    return 0;
}
