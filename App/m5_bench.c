#include "m5_bench.h"
#if APP_M5_STRESS_ENABLED
#include "bsp_services.h"
#include "tb6612_fan.h"
#include "storage.h"
#include "bus_arbiter.h"
#include "w25q64.h"
#include "FreeRTOS.h"
#include "task.h"
volatile uint32_t m5_bench_request;
static unsigned mode,phase,passed,failures,id_errors;
static uint32_t started,previous_seq,run_spi_waits,run_fan_waits;
static bus_status_t before;
/* LogTask calls this while holding the complete SPI transaction. */
void m5_bench_storage_hook(unsigned value)
{
    if(value==2u){
        fan_set(60);
        /* Force real overlap, without suspending scheduler or interrupts. */
        vTaskDelay(pdMS_TO_TICKS(40));
    }
}
static void finish(void)
{
    bus_status_t after;
    fan_set(0);fan_poll(HAL_GetTick());bus_status(&after);
    LOGI("M5 cross mode=%u pass=%u/100 errors=%u jedec_errors=%u spi_waits=%lu fan_waits=%lu",
        mode,passed,failures,id_errors,(unsigned long)(after.spi_waits-run_spi_waits),(unsigned long)(after.fan_waits-run_fan_waits));
    mode=phase=0;
}
void m5_bench_poll(void)
{
    event_record_t event;unsigned count;uint32_t id=0,now=HAL_GetTick();
    bus_status_t bus;
    if(m5_bench_request==3u){m5_bench_request=0;if(mode){++failures;finish();}return;}
    if(!mode){
        if(m5_bench_request!=1u && m5_bench_request!=2u)return;
        mode=m5_bench_request;m5_bench_request=0;
        bus_status(&bus);run_spi_waits=bus.spi_waits;run_fan_waits=bus.fan_waits;
        passed=failures=id_errors=phase=0;fan_set(0);fan_poll(now);
    }
    if(bsp_is_faulted()){++failures;finish();return;}
    bus_status(&bus);
    if(!phase){
        before=bus;started=now;
        if(storage_recent(0,&event,&count)){++failures;finish();return;}
        previous_seq=event.seq;
        if(mode==1u){if(fan_set(60)){++failures;finish();return;}phase=1;}
        else if(!storage_event(EVENT_BUS_TEST,(int32_t)passed+1,2))phase=3;
        else {++failures;finish();return;}
    }else if(now-started>5000u){++failures;finish();return;}
    else if(phase==1u && fan_percent()==60u){
        before=bus;
        if(storage_event(EVENT_BUS_TEST,(int32_t)passed+1,1)){++failures;finish();return;}
        phase=2;
    }else if(phase==2u && bus.spi_waits>before.spi_waits){
        fan_set(0);fan_poll(now);phase=4;
    }else if(phase==3u && fan_percent()==60u){
        if(bus.fan_waits==before.fan_waits){++failures;finish();return;}
        fan_set(0);fan_poll(now);phase=4;
    }else if(phase==4u && !storage_recent(0,&event,&count) &&
        (int32_t)(event.seq-previous_seq)>0 && event.type==EVENT_BUS_TEST &&
        event.parameter==(int32_t)passed+1 && event.auxiliary==(int32_t)mode){
        if(w25q_probe(&id) || id!=W25Q_JEDEC_ID){++id_errors;++failures;finish();return;}
        if(++passed==APP_M5_STRESS_CYCLES)finish();else phase=0;
    }
}
#endif
