"""Host model for the M6 ESP8266/MQTT boundary.

The test compiles the real Middleware/esp8266/esp8266.c with small deterministic
stubs for HAL, FreeRTOS, storage and DataHub.  It does not claim that an ESP8266
or broker is present; those checks belong to the hardware acceptance procedure.
"""
from pathlib import Path
import os
import subprocess
import textwrap


ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "tmp/m6-test"
OUT.mkdir(parents=True, exist_ok=True)
ENV = dict(os.environ)
ENV["PATH"] = "D:/msys64/ucrt64/bin;" + ENV.get("PATH", "")


def write(name: str, body: str) -> None:
    (OUT / name).write_text(textwrap.dedent(body), encoding="utf-8")


write(
    "app_config.h",
    r"""
    #ifndef TEST_APP_CONFIG_H
    #define TEST_APP_CONFIG_H
    #define APP_CFG_SSID 33u
    #define APP_CFG_PASSWORD 65u
    #define APP_CFG_HOST 65u
    #define APP_CFG_USER 33u
    #define APP_CFG_DEVICE 33u
    #define APP_NET_TX_SIZE 256u
    #define APP_EDIT_MAX_TEMP 100
    #define APP_EDIT_MAX_DISTANCE 500
    #define APP_EDIT_MAX_CONFIRM_MS 60000
    #define APP_MQTT_KEEPALIVE 60u
    #define APP_MQTT_LWT "offline"
    #define APP_TELEMETRY_MS 2000u
    #define ESP_RETRY_MAX 3u
    #define ESP_BACKOFF_MS 60000u
    #define CFG_TEMP 0u
    #define CFG_DISTANCE 1u
    #define CFG_SMOKE 2u
    #define CFG_AIR 3u
    #define CFG_MODE 4u
    #define STORAGE_NET_SSID 0u
    #define STORAGE_NET_PASSWORD 1u
    #define STORAGE_NET_HOST 2u
    #define STORAGE_NET_USER 3u
    #define STORAGE_NET_MQTT_PASSWORD 4u
    #define STORAGE_NET_DEVICE 5u
    #define APP_CFG_SSID 33u
    #define APP_CFG_PASSWORD 65u
    #endif
    """,
)
write(
    "stm32f1xx_hal.h",
    r"""
    #ifndef TEST_HAL_H
    #define TEST_HAL_H
    #include <stdint.h>
    uint32_t HAL_GetTick(void);
    unsigned __get_IPSR(void);
    #endif
    """,
)
write(
    "FreeRTOS.h",
    r"""
    #ifndef TEST_FREERTOS_H
    #define TEST_FREERTOS_H
    #define taskENTER_CRITICAL() ((void)0)
    #define taskEXIT_CRITICAL() ((void)0)
    #endif
    """,
)
write("task.h", "void vTaskDelay(unsigned ticks);\n")
write(
    "bsp_services.h",
    r"""
    #ifndef TEST_BSP_SERVICES_H
    #define TEST_BSP_SERVICES_H
    #include <stddef.h>
    #include <stdint.h>
    int bsp_net_send(const void *, size_t);
    char *bsp_net_tx_buffer(size_t *);
    int bsp_net_send_buffer(size_t);
    int bsp_net_hard_reset(void);
    void bsp_console_line(const char *,size_t);
    void bsp_console_release(void);
    int bsp_console_wifi_text(char *,char *);
    void bsp_console_wifi_result(const char *);
    uint32_t rtc_get(void);
    void bsp_log(const char *, const char *, ...);
    #define LOGI(...) bsp_log("I", __VA_ARGS__)
    #define LOGW(...) bsp_log("W", __VA_ARGS__)
    #define LOGE(...) bsp_log("E", __VA_ARGS__)
    #endif
    """,
)
write(
    "storage.h",
    r"""
    #ifndef TEST_STORAGE_H
    #define TEST_STORAGE_H
    #include <stddef.h>
    #include <stdint.h>
    int storage_get_network_text(unsigned, char *, size_t);
    int storage_get_network_numbers(uint32_t *, uint32_t *);
    int storage_set_wifi(void);
    #define CFG_FIELD_COUNT 5
    #define EVENT_TIME_REQUEST 8u
    typedef struct { uint32_t ready,errors,dropped,erases,config_seq,pending; } storage_status_t;
    int storage_get_fields(int32_t [CFG_FIELD_COUNT],storage_status_t *);
    #endif
    """,
)
write("alarm_engine.h", """
    #define FAULT_ESP 128u
    typedef struct { unsigned faults,transitions,warmup_remaining_ms;unsigned char state,kind; } alarm_status_t;
    void alarm_selftest_report(unsigned mask,int passed);
    void alarm_get_status(alarm_status_t *status);
""")
write(
    "sensor.h",
    r"""
    #ifndef TEST_SENSOR_H
    #define TEST_SENSOR_H
    #include <stdint.h>
    typedef enum { SENSOR_GOOD, SENSOR_STALE, SENSOR_FAULT } sensor_quality_t;
    typedef struct {
        int32_t value;
        sensor_quality_t quality;
        uint32_t timestamp;
        int32_t auxiliary;
        sensor_quality_t auxiliary_quality;
        uint32_t flags;
        uint32_t warmup_remaining_ms;
    } sensor_value_t;
    enum { SENSOR_DHT11, SENSOR_DISTANCE, SENSOR_SMOKE, SENSOR_AIR,
           SENSOR_RADAR, SENSOR_MCU, SENSOR_COUNT };
    #endif
    """,
)
write(
    "datahub.h",
    r"""
    #ifndef TEST_DATAHUB_H
    #define TEST_DATAHUB_H
    #include "sensor.h"
    typedef struct { sensor_value_t sensors[SENSOR_COUNT]; uint32_t captured_at; }
        datahub_snapshot_t;
    int datahub_snapshot(datahub_snapshot_t *);
    #endif
    """,
)

write(
    "test.c",
    r"""
    #include <assert.h>
    #include <stdarg.h>
    #include <stdio.h>
    #include <string.h>
    #include "esp8266.h"
    #include "datahub.h"
    #include "storage.h"
    #include "alarm_engine.h"

    static uint32_t now;
    static char tx[256];
    static size_t tx_len;
    static unsigned tx_count, reset_count;
    static int in_isr;

    uint32_t HAL_GetTick(void) { return now; }
    unsigned __get_IPSR(void) { return in_isr ? 1u : 0u; }
    void vTaskDelay(unsigned ticks) { now += ticks; }
    void bsp_log(const char *level, const char *format, ...) {
        (void)level; (void)format;
    }

    int bsp_net_send(const void *data, size_t length) {
        assert(length < sizeof(tx));
        memcpy(tx, data, length);
        tx[length] = 0;
        tx_len = length;
        ++tx_count;
        return 0;
    }
    char *bsp_net_tx_buffer(size_t *capacity) {
        if (capacity) *capacity = sizeof(tx);
        return tx;
    }
    int bsp_net_send_buffer(size_t length) {
        assert(length < sizeof(tx));
        tx[length] = 0;
        tx_len = length;
        ++tx_count;
        return 0;
    }
    int bsp_net_hard_reset(void) { ++reset_count; return 0; }
    void bsp_console_line(const char *s,size_t n){assert(s&&n);}
    void bsp_console_release(void){}
    static char provision_ssid[33]="new-ssid",provision_password[65]="new-password";
    int bsp_console_wifi_text(char *ssid,char *password){
        strcpy(ssid,provision_ssid);strcpy(password,provision_password);return 0;
    }
    void bsp_console_wifi_result(const char *s){assert(s);}

    int storage_get_network_text(unsigned field, char *out, size_t capacity) {
        static const char *values[] = {
            "test-ssid", "test-password", "broker.example", "user", "secret",
            "dev01"
        };
        if (!out || field >= sizeof(values) / sizeof(values[0]) || capacity < 2u)
            return -1;
        strncpy(out, values[field], capacity - 1u);
        out[capacity - 1u] = 0;
        return 0;
    }
    int storage_get_network_numbers(uint32_t *port, uint32_t *keepalive) {
        if (!port || !keepalive) return -1;
        *port = 1883u; *keepalive = 60u; return 0;
    }
    static uint32_t config_seq=1;
    int storage_set_wifi(void){++config_seq;return 0;}
    uint32_t rtc_get(void) { return 1234u; }
    void alarm_selftest_report(unsigned mask,int passed){assert(mask==128u && passed);}
    void alarm_get_status(alarm_status_t *s){memset(s,0,sizeof(*s));s->state=2;}
    int storage_get_fields(int32_t v[CFG_FIELD_COUNT],storage_status_t *s){
        static const int32_t values[]={50,100,3000,10000,1};
        memcpy(v,values,sizeof(values));memset(s,0,sizeof(*s));
        s->ready=1;s->config_seq=config_seq;return 0;
    }
    static int telemetry_temp=20;
    int datahub_snapshot(datahub_snapshot_t *out) {
        unsigned i;
        if (!out) return -1;
        memset(out, 0, sizeof(*out));
        for (i = 0; i < SENSOR_COUNT; ++i) {
            out->sensors[i].quality = SENSOR_GOOD;
            out->sensors[i].value = (int32_t)(20 + i);
        }
        out->sensors[SENSOR_DHT11].auxiliary = 550;
        out->sensors[SENSOR_DHT11].value=telemetry_temp;
        return 0;
    }

    void net_on_rx_line(const char *, size_t);
    void net_on_rx_bytes(const unsigned char *, size_t);
    void net_on_rx_prompt(void);

    static void line(const char *value) {
        net_on_rx_line(value, strlen(value));
    }
    static void bytes(const unsigned char *value, size_t length) {
        net_on_rx_bytes(value, length);
    }
    static void command_round(const char *needle, const char *response) {
        size_t before = tx_count;
        network_poll(now);
        assert(tx_count == before + 1u);
        assert(strstr(tx, needle) != NULL);
        line(response);
        network_poll(now);
    }
    static void raw_round(unsigned char packet_type, const unsigned char *response,
                          size_t response_len) {
        size_t before = tx_count;
        network_poll(now);
        assert(tx_count == before + 1u);
        assert(strstr(tx, "AT+CIPSEND=") == tx);
        net_on_rx_prompt();
        network_poll(now);
        assert(tx_count == before + 2u && tx_len > 1u);
        assert(((unsigned char)tx[0] >> 4) == packet_type);
        line("SEND OK");
        network_poll(now);
        if (response && response_len) {
            bytes(response, response_len);
            network_poll(now);
        }
    }
    static void mqtt_publish(const char *topic, const char *payload) {
        unsigned char packet[160];
        size_t topic_len = strlen(topic), payload_len = strlen(payload);
        size_t remaining = 2u + topic_len + payload_len, n = 0u;
        assert(topic_len < 100u && remaining < 128u);
        packet[n++] = 0x30u;
        packet[n++] = (unsigned char)remaining;
        packet[n++] = (unsigned char)(topic_len >> 8);
        packet[n++] = (unsigned char)topic_len;
        memcpy(packet + n, topic, topic_len); n += topic_len;
        memcpy(packet + n, payload, payload_len); n += payload_len;
        bytes(packet, n);
    }
    static int has_bytes(const char *needle) {
        size_t i, n = strlen(needle);
        for (i = 0; i + n <= tx_len; ++i)
            if (!memcmp(tx + i, needle, n)) return 1;
        return 0;
    }
    static long find_bytes(const char *needle) {
        size_t i, n = strlen(needle);
        for (i = 0; i + n <= tx_len; ++i)
            if (!memcmp(tx + i, needle, n)) return (long)i;
        return -1;
    }
    static void drain_outbound(void) {
        unsigned i;
        for (i = 0; i < 64u; ++i) {
            size_t before = tx_count;
            network_poll(now);
            if (tx_count == before) break;
            net_on_rx_prompt();
            network_poll(now);
            line("SEND OK");
            network_poll(now);
        }
    }
    static void reach_online(void) {
        network_poll(now);                 /* INIT -> RESET */
        network_poll(now);                 /* reset -> AT */
        command_round("AT\r\n", "OK");
        line("AT version 1.0");
        command_round("AT+GMR", "OK");
        command_round("AT+CWMODE=1", "OK");
        line("WIFI CONNECTED");
        command_round("AT+CWJAP=", "OK");
        command_round("AT+CIFSR", "+CIFSR:STAIP,\"192.0.2.7\"");
        line("OK");
        network_poll(now);
        line("+CWJAP:\"test-ssid\",\"aa:bb:cc:dd:ee:ff\",6,-55");
        command_round("AT+CWJAP?", "OK");
        command_round("AT+CIPMUX=0", "OK");
        command_round("AT+CIPMODE=0", "OK");
        command_round("AT+CIPSTART=", "CONNECT");
        {
            static const unsigned char connack[] = {0x20,0x02,0x00,0x00};
            raw_round(1u, connack, sizeof(connack));
        }
        {
            static const unsigned char suback[] = {0x90,0x03,0x00,0x01,0x00};
            raw_round(8u, suback, sizeof(suback));
        }
        {
            network_status_t s;
            assert(network_get_status(&s) == 0);
            assert(s.wifi && s.mqtt && s.rssi == -55);
            assert(strcmp(s.ip, "192.0.2.7") == 0);
            assert(strstr(s.version, "AT version") != NULL);
        }
    }

    int main(void) {
        network_status_t status;
        network_command_t command;
        unsigned i;

        network_init();
        assert(network_get_status(&status) == 0);
        assert(!status.wifi && !status.mqtt && status.rssi == -127);

        /* API rejects ISR-originated events and preserves a bounded backlog. */
        in_isr = 1;
        assert(network_event_enqueue(1u, 2, 3, 4u) == -1);
        in_isr = 0;
        for (i = 0; i < 20u; ++i)
            assert(network_event_enqueue(i + 1u, (int32_t)i, -(int32_t)i, i) == 0);
        assert(network_get_status(&status) == 0);
        assert(status.dropped == 0u);

        reach_online();

        /* MQTT 3.1.1 downlink parser accepts all requested commands. */
        mqtt_publish("smshub/dev01/cmd", "fan=45");
        assert(network_command_pop(&command) == 0);
        assert(command.kind == NETWORK_CMD_FAN && command.value == 45);
        mqtt_publish("smshub/dev01/cmd", "servo=180");
        assert(network_command_pop(&command) == 0);
        assert(command.kind == NETWORK_CMD_SERVO && command.value == 180);
        mqtt_publish("smshub/dev01/cmd", "arm=1");
        assert(network_command_pop(&command) == 0);
        assert(command.kind == NETWORK_CMD_ARM && command.field == 4u &&
               command.value == 1);
        mqtt_publish("smshub/dev01/cmd", "th=temp:52");
        assert(network_command_pop(&command) == 0);
        assert(command.kind == NETWORK_CMD_THRESHOLD && command.field == 0u &&
               command.value == 52);
        mqtt_publish("smshub/dev01/cmd", "reboot=1");
        assert(network_command_pop(&command) == 0);
        assert(command.kind == NETWORK_CMD_REBOOT);
        mqtt_publish("smshub/dev01/cmd","ack=1");
        assert(!network_command_pop(&command) && command.kind==NETWORK_CMD_ACK);
        mqtt_publish("smshub/dev01/cmd","time=1800000000");
        assert(!network_command_pop(&command) && command.kind==NETWORK_CMD_TIME);
        mqtt_publish("smshub/dev01/cmd","time=0");assert(network_command_pop(&command)==-1);
        mqtt_publish("smshub/dev01/cmd","th=temp:101");assert(network_command_pop(&command)==-1);
        mqtt_publish("smshub/dev01/cmd","th=air:60001");assert(network_command_pop(&command)==-1);
        mqtt_publish("smshub/dev01/cmd","ack=00000000000000000000000000000000000000000000000001junk");
        assert(network_command_pop(&command)==-1);
        mqtt_publish("smshub/dev01/cmd", "fan=101");
        assert(network_command_pop(&command) == -1);
        mqtt_publish("smshub/dev01/cmd", "servo=-1");
        assert(network_command_pop(&command) == -1);

        /* A reconnect starts with a clean MQTT parser even if the old TCP
         * connection ended halfway through a PUBLISH body. */
        {
            const unsigned char partial[] = {0x30, 0x12, 0x00, 0x10, 's'};
            bytes(partial, sizeof(partial));
            line("CLOSED");
            network_poll(now);
            network_init();
            reach_online();
        }

        /* Finish any retained event publishes before checking telemetry. */
        drain_outbound();
        ++config_seq;
        raw_round(3u,NULL,0);
        assert(has_bytes("/config") && has_bytes("\"seq\":2") && has_bytes("\"temp\":50"));
        assert((unsigned char)tx[0]==0x31u); /* retained committed config */
        /* A fresh telemetry transaction is fixed-order and contains valid JSON. */
        now += 2000u;
        telemetry_temp=9;
        network_poll(now);
        assert(strstr(tx, "AT+CIPSEND=") == tx);
        telemetry_temp=1000; /* Different digits before prompt must not rebuild. */
        net_on_rx_prompt();
        network_poll(now);
        assert(has_bytes("\"ts\":") && has_bytes("\"temp\":"));
        assert(has_bytes("\"temp\":9,"));
        assert(find_bytes("\"ts\":") < find_bytes("\"temp\":"));
        assert(find_bytes("\"temp\":") < find_bytes("\"hum\":"));
        assert(find_bytes("\"hum\":") < find_bytes("\"distance\":"));
        assert(find_bytes("\"distance\":") < find_bytes("\"human\":"));
        assert(find_bytes("\"human\":") < find_bytes("\"smoke\":"));
        assert(find_bytes("\"smoke\":") < find_bytes("\"air\":"));
        assert(find_bytes("\"air\":") < find_bytes("\"wifi\":"));
        assert(find_bytes("\"wifi\":") < find_bytes("\"mqtt\":"));
        assert(find_bytes("\"mqtt\":") < find_bytes("\"mode\":"));
        assert(has_bytes("\"state\":2") && has_bytes("\"fault\":0") && has_bytes("\"q\":31"));
        line("SEND OK");
        network_poll(now);

        /* A rejected TCP setup retries/reset without claiming Wi-Fi/MQTT online. */
        network_init();
        network_poll(now);
        network_poll(now);
        command_round("AT\r\n", "OK");
        command_round("AT+GMR", "OK");
        {
            unsigned before_reset = reset_count;
            for (i = 0; i < 3u; ++i)
                command_round("AT+CWMODE=1", "ERROR");
            network_poll(now);
            assert(reset_count == before_reset + 1u);
            assert(network_get_status(&status) == 0);
            assert(!status.wifi && !status.mqtt);
        }

        puts("PASS M6 host model: API init, ISR guard, 20-event cache, TCP/MQTT online, "
             "binary downlink bounds, fixed telemetry field order, TCP setup rejection/reset");
        return 0;
    }
    """,
)

gcc = Path("D:/msys64/ucrt64/bin/gcc.exe")
if not gcc.exists():
    gcc = Path("gcc")
args = [str(gcc), "-std=c99", "-Wall", "-Wextra", "-Werror"]
args += ["-I", "tmp/m6-test", "-I", "Middleware/esp8266"]
args += ["tmp/m6-test/test.c", "Middleware/esp8266/esp8266.c"]
args += ["-o", "tmp/m6-test/test.exe"]
subprocess.run(args, cwd=ROOT, env=ENV, check=True)
subprocess.run(["tmp/m6-test/test.exe"], cwd=ROOT, env=ENV, check=True)
