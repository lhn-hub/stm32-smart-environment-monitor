#include "app_time.h"
#include "bsp_services.h"
#include "FreeRTOS.h"
#include "task.h"
#include <string.h>
static volatile uint32_t pending;
static volatile unsigned source;
uint32_t app_time_build_epoch(const char *date,const char *time)
{
    static const char months[]="JanFebMarAprMayJunJulAugSepOctNovDec";
    static const uint16_t days_before[]={0,31,59,90,120,151,181,212,243,273,304,334};
    uint32_t year,month,day,days,y,hour,minute,second;
    if(!date || !time || strlen(date)!=11u || strlen(time)!=8u)return 0;
    for(month=0;month<12u;++month)if(!memcmp(date,months+month*3u,3))break;
    year=(uint32_t)(date[7]-'0')*1000u+(uint32_t)(date[8]-'0')*100u+
         (uint32_t)(date[9]-'0')*10u+(uint32_t)(date[10]-'0');
    day=(date[4]==' '?0u:(uint32_t)(date[4]-'0')*10u)+(uint32_t)(date[5]-'0');
    hour=(uint32_t)(time[0]-'0')*10u+(uint32_t)(time[1]-'0');
    minute=(uint32_t)(time[3]-'0')*10u+(uint32_t)(time[4]-'0');
    second=(uint32_t)(time[6]-'0')*10u+(uint32_t)(time[7]-'0');
    if(year<2020u || year>2037u || month>=12u || !day || day>31u ||
       hour>23u || minute>59u || second>59u)return 0;
    days=0;
    for(y=1970u;y<year;++y)days+=365u+(y%4u==0u && (y%100u!=0u || y%400u==0u));
    days+=days_before[month]+day-1u;
    if(month>1u && year%4u==0u)++days;
    /* Build tools use the project's UTC+8 local time; RTC stores UTC seconds. */
    return days*86400u+hour*3600u+minute*60u+second-8u*3600u;
}
void app_time_start(void)
{
    uint32_t now=rtc_get();
    if(now>=1577836800u && now<=2147483647u)source=TIME_BACKUP;
    else{
        uint32_t build=app_time_build_epoch(__DATE__,__TIME__);
        source=build && !rtc_set(build)?TIME_BUILD:TIME_UNSET;
    }
    LOGI("RTC时间来源=%u UTC=%lu",source,(unsigned long)rtc_get());
}
int app_time_request(uint32_t seconds)
{
    int result=-1;
    if(seconds<1577836800u || seconds>2147483647u)return -1;
    taskENTER_CRITICAL();if(!pending){pending=seconds;result=0;}taskEXIT_CRITICAL();
    return result;
}
void app_time_poll(void)
{
    uint32_t seconds;
    taskENTER_CRITICAL();seconds=pending;pending=0;taskEXIT_CRITICAL();
    if(seconds){
        if(!rtc_set(seconds)){source=TIME_GATEWAY;LOGI("RTC网关校时 UTC=%lu",(unsigned long)seconds);}
        else LOGW("RTC校时失败，保留原时间");
    }
}
unsigned app_time_source(void){return source;}
