#include "eventlog.h"
#include "kvstore.h"
#include "w25q64.h"
#include <stddef.h>
#include <string.h>
typedef char event_size_check[sizeof(event_record_t)==LOG_ITEM_SIZE?1:-1];
static uint32_t cursor,next_sequence,newest_position;
static uint16_t sector_records[FLASH_LOG_SECTORS];
static uint32_t available;
static int mounted,has_record;
static uint32_t count(void){return FLASH_LOG_SIZE/LOG_ITEM_SIZE;}
static int blank(const event_record_t *r)
{unsigned i;const uint8_t *p=(const uint8_t*)r;for(i=0;i<sizeof(*r);i++)if(p[i]!=0xff)return 0;return 1;}
static int valid(const event_record_t *r)
{return r->magic==APP_EVENT_MAGIC && r->commit==APP_RECORD_COMMIT && r->crc32==storage_crc32(r,offsetof(event_record_t,crc32));}
static int read_at(uint32_t index,event_record_t *r){return w25q_read(FLASH_LOG_ADDR+index*LOG_ITEM_SIZE,r,sizeof(*r));}
int eventlog_init(void)
{
    event_record_t r;uint32_t i,max_seq=0,last_used=0;int used=0;
    mounted=has_record=0;cursor=0;next_sequence=1;newest_position=0;available=0;
    memset(sector_records,0,sizeof(sector_records));
    for(i=0;i<count();i++){
        if(read_at(i,&r))return -1;
        if(!blank(&r)){used=1;last_used=i;}
        if(valid(&r)){
            ++available;++sector_records[i/(W25Q_SECTOR_SIZE/LOG_ITEM_SIZE)];
            if(!has_record||storage_seq_newer(r.seq,max_seq)){has_record=1;max_seq=r.seq;newest_position=i;}
        }
    }
    if(has_record){next_sequence=max_seq+1u;cursor=(newest_position+1u)%count();}
    else if(used){if(last_used+1u>=count())return -1;cursor=last_used+1u;}
    mounted=1;return 0;
}
uint32_t eventlog_next_seq(void){return next_sequence;}
int log_append(uint32_t time,uint32_t type,int32_t parameter,int32_t auxiliary)
{
    event_record_t r,check;uint32_t tries,address,marker=APP_RECORD_COMMIT;
    if(!mounted)return -1;
    for(tries=0;tries<count();tries++){
        address=FLASH_LOG_ADDR+cursor*LOG_ITEM_SIZE;
        if(address%W25Q_SECTOR_SIZE==0){
            /* Do not destroy the newest valid record if corrupted media wraps
             * all the way back to it. No separate hot pointer sector. */
            if(has_record && cursor/(W25Q_SECTOR_SIZE/LOG_ITEM_SIZE)==newest_position/(W25Q_SECTOR_SIZE/LOG_ITEM_SIZE))goto uncertain;
            if(w25q_erase(address))goto uncertain;
            available-=sector_records[cursor/(W25Q_SECTOR_SIZE/LOG_ITEM_SIZE)];
            sector_records[cursor/(W25Q_SECTOR_SIZE/LOG_ITEM_SIZE)]=0;
        }
        if(read_at(cursor,&check))goto uncertain;
        if(blank(&check))break;
        cursor=(cursor+1u)%count(); /* torn uncommitted slot is never overwritten */
    }
    if(tries==count())goto uncertain;
    memset(&r,0,sizeof(r));r.magic=APP_EVENT_MAGIC;r.seq=next_sequence;r.unix_time=time;r.type=type;
    r.parameter=parameter;r.auxiliary=auxiliary;r.crc32=storage_crc32(&r,offsetof(event_record_t,crc32));r.commit=0xffffffffu;
    if(w25q_program(address,&r,offsetof(event_record_t,commit)) ||
       w25q_program(address+offsetof(event_record_t,commit),&marker,sizeof(marker)) ||
       read_at(cursor,&check) || !valid(&check))goto uncertain;
    ++available;++sector_records[cursor/(W25Q_SECTOR_SIZE/LOG_ITEM_SIZE)];
    newest_position=cursor;has_record=1;++next_sequence;cursor=(cursor+1u)%count();return 0;
uncertain:
    mounted=0;return -1;
}
int log_read_last(size_t n,event_record_t *out,size_t *read_count)
{
    event_record_t r;uint32_t i,pos;size_t found=0;
    if(!mounted || !out || !read_count || n>count())return -1;
    /* Startup scans once. With fewer than20 records, stop after actual count
     * rather than repeatedly reading empty64KB after each appended event. */
    if(n>available)n=available;
    if(has_record){pos=newest_position;for(i=0;i<count()&&found<n;i++){
        if(read_at(pos,&r))return -1;
        if(valid(&r))out[found++]=r;
        pos=pos?pos-1u:count()-1u;
    }}
    *read_count=found;return 0;
}
