#include "ec11.h"
#include "app_config.h"
#include "FreeRTOS.h"
#include "task.h"
static uint8_t previous;
static int phase;
static volatile uint8_t ring[APP_EC11_RING_SIZE];
static volatile uint32_t head,tail,lost;
static uint32_t seen_lost;
static TaskHandle_t owner;
void ec11_reset(uint8_t ab) { previous=ab&3u; phase=0; }
int ec11_feed(uint8_t ab)
{
    static const int8_t step[16]={0,-1,1,0,1,0,0,-1,-1,0,0,1,0,1,-1,0};
    uint8_t changed=previous^(ab&3u);
    int d=step[(previous<<2)|(ab&3u)];
    previous=ab&3u;
    if(changed==3u) { phase=0; return 0; }
    phase+=d;
    if(phase>=APP_EC11_EDGES_PER_DETENT) { phase=0; return 1; }
    if(phase<=-APP_EC11_EDGES_PER_DETENT) { phase=0; return -1; }
    return 0;
}
static uint8_t sample(void)
{
    uint32_t idr=EC11_A_PORT->IDR;
    return ((idr&EC11_A_PIN)?2u:0u)|((idr&EC11_B_PIN)?1u:0u);
}
void ec11_start(void)
{
    ec11_reset(sample()); tail=head; seen_lost=lost;
    owner=xTaskGetCurrentTaskHandle();
}
void ec11_irq(void)
{
    BaseType_t wake=pdFALSE;
    uint32_t next=(head+1u)%APP_EC11_RING_SIZE;
    if(!owner) return;
    /* Ordered AB snapshots are needed: a notification bit alone loses edges.
     * ISR captures only, MenuTask owns the quadrature state machine. */
    if(next==tail) ++lost;
    else { ring[head]=sample(); __DMB(); head=next; }
    vTaskNotifyGiveFromISR(owner,&wake);
    portYIELD_FROM_ISR(wake);
}
int ec11_poll(void)
{
    int total=0;
    (void)ulTaskNotifyTake(pdTRUE,0);
    if(seen_lost!=lost) {
        taskENTER_CRITICAL(); tail=head; seen_lost=lost; ec11_reset(sample()); taskEXIT_CRITICAL();
        return 0;
    }
    while(tail!=head) {
        uint8_t ab=ring[tail]; tail=(tail+1u)%APP_EC11_RING_SIZE;
        total+=ec11_feed(ab);
    }
    return total;
}
uint32_t ec11_overflows(void) { return lost; }
