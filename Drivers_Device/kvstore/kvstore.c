#include "kvstore.h"
#include "w25q64.h"
#include <stddef.h>
#include <string.h>
typedef char config_size_check[sizeof(config_t)==KV_RECORD_SIZE?1:-1];
typedef char commit_offset_check[offsetof(config_t,commit)==KV_RECORD_SIZE-4u?1:-1];
typedef char bank_alignment_check[FLASH_KV_ADDR_B-FLASH_KV_ADDR_A>=W25Q_SECTOR_SIZE?1:-1];
typedef char defaults_fit[(sizeof(APP_WIFI_SSID)<=APP_CFG_SSID && sizeof(APP_WIFI_PASSWORD)<=APP_CFG_PASSWORD &&
    sizeof(APP_MQTT_HOST)<=APP_CFG_HOST && sizeof(APP_MQTT_USER)<=APP_CFG_USER &&
    sizeof(APP_MQTT_PASSWORD)<=APP_CFG_PASSWORD && sizeof(APP_DEVICE_DEFAULT)<=APP_CFG_DEVICE)?1:-1];
static config_t work;
static uint32_t slots[2],sequence,latest_bank;
static int loaded;
uint32_t storage_crc32(const void *p,uint32_t n)
{
    const uint8_t *b=p;uint32_t crc=0xffffffffu;unsigned k;
    while(n--){crc^=*b++;for(k=0;k<8;k++)crc=(crc>>1)^((0u-(crc&1u))&0xedb88320u);}
    return ~crc;
}
int storage_seq_newer(uint32_t a,uint32_t b){return (int32_t)(a-b)>0;}
void config_defaults(config_t *c)
{
    memset(c,0,sizeof(*c));c->magic=APP_CONFIG_MAGIC;c->version=APP_CONFIG_VERSION;
    c->temp_x10=DEF_TEMP_ALARM_C_X10;c->distance_cm=DEF_INTRUSION_DIST_CM;
    c->smoke_ms=DEF_SMOKE_CONFIRM_MS;c->air_ms=DEF_AIR_CONFIRM_MS;
    c->intrusion_ms=DEF_INTRUSION_CONFIRM_MS;c->release_ms=DEF_ALARM_RELEASE_MS;
    c->telemetry_ms=DEF_TELEMETRY_PERIOD_MS;c->mode=APP_MODE_DEFAULT;
    c->mqtt_port=APP_MQTT_PORT;c->mqtt_keepalive=APP_MQTT_KEEPALIVE;
    strcpy(c->ssid,APP_WIFI_SSID);strcpy(c->wifi_password,APP_WIFI_PASSWORD);
    strcpy(c->mqtt_host,APP_MQTT_HOST);strcpy(c->mqtt_user,APP_MQTT_USER);
    strcpy(c->mqtt_password,APP_MQTT_PASSWORD);strcpy(c->device,APP_DEVICE_DEFAULT);
}
int config_valid(const config_t *c)
{
    return c && c->magic==APP_CONFIG_MAGIC && c->version==APP_CONFIG_VERSION &&
      c->temp_x10>=0 && c->temp_x10<=APP_EDIT_MAX_TEMP*10 && c->distance_cm<=APP_EDIT_MAX_DISTANCE &&
      c->smoke_ms<=APP_EDIT_MAX_CONFIRM_MS && c->air_ms<=APP_EDIT_MAX_CONFIRM_MS &&
      c->intrusion_ms<=APP_EDIT_MAX_CONFIRM_MS && c->release_ms<=APP_EDIT_MAX_CONFIRM_MS &&
      c->telemetry_ms>0 && c->telemetry_ms<=APP_CONFIG_MAX_PERIOD_MS && c->mode<=1 &&
      c->mqtt_port>0 && c->mqtt_port<=65535u && c->mqtt_keepalive>0 && c->mqtt_keepalive<=65535u &&
      memchr(c->ssid,0,sizeof(c->ssid)) && memchr(c->wifi_password,0,sizeof(c->wifi_password)) &&
      memchr(c->mqtt_host,0,sizeof(c->mqtt_host)) && c->mqtt_host[0] &&
      memchr(c->mqtt_user,0,sizeof(c->mqtt_user)) && memchr(c->mqtt_password,0,sizeof(c->mqtt_password)) &&
      memchr(c->device,0,sizeof(c->device)) && c->device[0];
}
static uint32_t base(unsigned bank){return bank?FLASH_KV_ADDR_B:FLASH_KV_ADDR_A;}
static int read_record(uint32_t a,config_t *c)
{
    unsigned off;for(off=0;off<sizeof(*c);off+=W25Q_PAGE_SIZE)
        if(w25q_read(a+off,(uint8_t*)c+off,W25Q_PAGE_SIZE))return -1;
    return 0;
}
static int blank(const config_t *c)
{unsigned i;const uint8_t *b=(const uint8_t*)c;for(i=0;i<sizeof(*c);i++)if(b[i]!=0xff)return 0;return 1;}
static int committed(const config_t *c)
{return c->commit==APP_RECORD_COMMIT && config_valid(c) && c->crc32==storage_crc32(c,offsetof(config_t,crc32));}
int kv_load(config_t *out)
{
    unsigned bank,slot;int found=0;loaded=0;slots[0]=slots[1]=0;sequence=0;latest_bank=1;
    if(!out)return -1;
    for(bank=0;bank<2;bank++)for(slot=0;slot<KV_SLOTS_PER_BANK;slot++){
        if(read_record(base(bank)+slot*KV_RECORD_SIZE,&work))return -1;
        if(!blank(&work))slots[bank]=slot+1;
        if(committed(&work)&&(!found||storage_seq_newer(work.seq,sequence))){*out=work;sequence=work.seq;latest_bank=bank;found=1;}
    }
    loaded=1;
    if(found)return 0;
    /* New/torn-first-write media: append defaults in free space, never autoerase
     * unknown nonblank banks without a surviving valid record. */
    config_defaults(out);
    if(slots[0]>=KV_SLOTS_PER_BANK || slots[1]>=KV_SLOTS_PER_BANK){loaded=0;return -1;}
    if(kv_save(out)||kv_save(out)){loaded=0;return -1;}
    return 1;
}
int kv_save(config_t *c)
{
    unsigned bank=latest_bank^1u,off;uint32_t address,marker=APP_RECORD_COMMIT;size_t n;
    if(!loaded || !config_valid(c))return -1;
    if(slots[bank]>=KV_SLOTS_PER_BANK){
        /* Latest survives in opposite sector. Erase is never current-bank. */
        if(w25q_erase(base(bank))){loaded=0;return -1;}slots[bank]=0;
    }
    work=*c;work.seq=sequence+1u;work.commit=0xffffffffu;
    work.crc32=storage_crc32(&work,offsetof(config_t,crc32));
    address=base(bank)+slots[bank]*KV_RECORD_SIZE;
    for(off=0;off<offsetof(config_t,commit);off+=(unsigned)n){
        n=offsetof(config_t,commit)-off;if(n>W25Q_PAGE_SIZE)n=W25Q_PAGE_SIZE;
        if(w25q_program(address+off,(uint8_t*)&work+off,n))goto uncertain;
    }
    if(w25q_program(address+offsetof(config_t,commit),&marker,sizeof(marker)))goto uncertain;
    if(read_record(address,&work)||!committed(&work))goto uncertain;
    sequence=work.seq;latest_bank=bank;++slots[bank];*c=work;return 0;
uncertain:
    loaded=0;return -1; /* rescan before any further write; never blind retry */
}
