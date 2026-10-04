#include "bsp_services.h"
#include "bsp_timer_us.h"
#include "app_config.h"
#include "i2c.h"
#include "usart.h"
#include "FreeRTOS.h"
#include "task.h"
#include <string.h>
static uint8_t rx[ESP_RX_BUF_SIZE],tx[APP_NET_TX_SIZE];
static char line[APP_NET_LINE_SIZE];
static size_t read_pos,line_len;
static volatile uint32_t idle_events,uart_errors;
static volatile uint32_t uart_error_bits;
static uint32_t gap_count,error_count,error_bits,restart_failures,last_report;
static uint32_t last_poll;
static uint32_t tx_started;
static uint8_t discard;

int bsp_i2c_recover(void)
{
    GPIO_InitTypeDef gpio={0}; unsigned i;
    HAL_I2C_DeInit(&hi2c1);
    HAL_GPIO_WritePin(OLED_SCL_PORT,OLED_SCL_PIN|OLED_SDA_PIN,GPIO_PIN_SET);
    gpio.Pin=OLED_SCL_PIN|OLED_SDA_PIN; gpio.Mode=GPIO_MODE_OUTPUT_OD;
    gpio.Pull=GPIO_NOPULL; gpio.Speed=GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(OLED_SCL_PORT,&gpio);
    for(i=0;i<APP_I2C_RECOVERY_CLOCKS;i++) {
        if(HAL_GPIO_ReadPin(OLED_SDA_PORT,OLED_SDA_PIN)==GPIO_PIN_SET) break;
        HAL_GPIO_WritePin(OLED_SCL_PORT,OLED_SCL_PIN,GPIO_PIN_RESET);
        bsp_delay_us(APP_I2C_PULSE_US);
        HAL_GPIO_WritePin(OLED_SCL_PORT,OLED_SCL_PIN,GPIO_PIN_SET);
        bsp_delay_us(APP_I2C_PULSE_US);
        if(HAL_GPIO_ReadPin(OLED_SCL_PORT,OLED_SCL_PIN)==GPIO_PIN_RESET) goto fail;
    }
    HAL_GPIO_WritePin(OLED_SCL_PORT,OLED_SCL_PIN,GPIO_PIN_RESET);
    HAL_GPIO_WritePin(OLED_SDA_PORT,OLED_SDA_PIN,GPIO_PIN_RESET);
    bsp_delay_us(APP_I2C_PULSE_US);
    HAL_GPIO_WritePin(OLED_SCL_PORT,OLED_SCL_PIN,GPIO_PIN_SET);
    bsp_delay_us(APP_I2C_PULSE_US);
    HAL_GPIO_WritePin(OLED_SDA_PORT,OLED_SDA_PIN,GPIO_PIN_SET);
    if(HAL_GPIO_ReadPin(OLED_SCL_PORT,OLED_SCL_PIN)==GPIO_PIN_RESET ||
       HAL_GPIO_ReadPin(OLED_SDA_PORT,OLED_SDA_PIN)==GPIO_PIN_RESET) goto fail;
    MX_I2C1_Init(); return 0;
fail:
    MX_I2C1_Init(); return -1;
}
void i2c_scan(void)
{
    unsigned a,count=0;
    if(HAL_GPIO_ReadPin(OLED_SDA_PORT,OLED_SDA_PIN)==GPIO_PIN_RESET && bsp_i2c_recover()) {
        LOGE("I2C恢复后仍锁死"); return;
    }
    for(a=APP_I2C_FIRST_ADDR;a<=APP_I2C_LAST_ADDR;a++) {
        if(HAL_I2C_IsDeviceReady(&hi2c1,a<<1,1,APP_I2C_TIMEOUT_MS)==HAL_OK) {
            LOGI("I2C在线地址=0x%02X",a); ++count;
        }
        vTaskDelay(1);
    }
    LOGI("I2C扫描完成，设备数=%u",count);
}
void bsp_net_idle_irq(void)
{
    if(__HAL_UART_GET_FLAG(&huart3,UART_FLAG_IDLE) && __HAL_UART_GET_IT_SOURCE(&huart3,UART_IT_IDLE)) {
        __HAL_UART_CLEAR_IDLEFLAG(&huart3); ++idle_events;
    }
}
void HAL_UART_ErrorCallback(UART_HandleTypeDef *uart)
{
    if(uart==&huart3) { uart_error_bits|=uart->ErrorCode; ++uart_errors; }
}
int bsp_net_start(void)
{
    MX_USART3_UART_Init(); last_poll=HAL_GetTick();
    if(HAL_UART_Receive_DMA(&huart3,rx,sizeof(rx))!=HAL_OK) return -1;
    __HAL_UART_ENABLE_IT(&huart3,UART_IT_IDLE); return 0;
}
int bsp_net_send(const void *data,size_t length)
{
    /* NetTask is sole TX owner. Buffer lifetime extends until DMA completes. */
    if(!data || !length || length>sizeof(tx) || huart3.gState!=HAL_UART_STATE_READY) return -1;
    memcpy(tx,data,length);
    tx_started=HAL_GetTick();
    return HAL_UART_Transmit_DMA(&huart3,tx,(uint16_t)length)==HAL_OK?0:-1;
}
__weak void net_on_rx_line(const char *data,size_t length)
{ (void)length; LOGI("串口3接收：%s",data); }
void bsp_net_poll(void)
{
    uint32_t now=HAL_GetTick();
    uint32_t errors,bits;
    /* Only a few RAM accesses under BASEPRI; no IO/delay/logging. TIM4 runs.
     * Atomically take ISR diagnostics so a new error during restart survives.
     */
    taskENTER_CRITICAL();
    errors=uart_errors; bits=uart_error_bits;
    uart_errors=0; uart_error_bits=0;
    taskEXIT_CRITICAL();
    if(huart3.gState!=HAL_UART_STATE_READY && now-tx_started>=APP_NET_TX_TIMEOUT_MS) {
        HAL_UART_AbortTransmit(&huart3); LOGE("串口3发送DMA超时");
    }
    size_t write_pos=sizeof(rx)-__HAL_DMA_GET_COUNTER(huart3.hdmarx);
    write_pos%=sizeof(rx);
    /* At115200 8N1, full512B ring lasts ~44ms. Conservative scheduling
     * gap guard discards uncertain data instead of accepting corrupted lines.
     */
    if(now-last_poll>APP_NET_RING_GUARD_MS || errors) {
        read_pos=write_pos; line_len=0; discard=1;
        if(now-last_poll>APP_NET_RING_GUARD_MS) ++gap_count;
        error_count+=errors; error_bits|=bits;
        if(errors) {
            HAL_UART_AbortReceive(&huart3);
            if(HAL_UART_Receive_DMA(&huart3,rx,sizeof(rx))!=HAL_OK) {
                ++restart_failures;
                /* Keep retry pending; do not silently leave RX stopped. */
                taskENTER_CRITICAL(); ++uart_errors; taskEXIT_CRITICAL();
            }
            read_pos=0; __HAL_UART_ENABLE_IT(&huart3,UART_IT_IDLE);
            write_pos=0; /* old DMA write index is invalid */
        }
    }
    last_poll=now;
    while(read_pos!=write_pos) {
        if(HAL_GetTick()-last_poll>APP_NET_RING_GUARD_MS) {
            /* A slow line callback can itself span a DMA lap. Drop the
             * uncertain suffix before touching any more ring bytes. */
            read_pos=(sizeof(rx)-__HAL_DMA_GET_COUNTER(huart3.hdmarx))%sizeof(rx);
            line_len=0; discard=1; ++gap_count; break;
        }
        char c=(char)rx[read_pos]; read_pos=(read_pos+1u)%sizeof(rx);
        if(c=='\n') {
            if(!discard) { line[line_len]=0; net_on_rx_line(line,line_len); }
            line_len=0; discard=0;
        } else if(c!='\r' && !discard) {
            if(line_len+1u<sizeof(line)) line[line_len++]=c;
            else { line_len=0; discard=1; }
        }
    }
    /* Report AFTER restoring/draining RX. Repeated errors must not produce
     * a polling/logging feedback loop. Keep the 30ms loss guard unchanged. */
    now=HAL_GetTick();
    if(now-last_report>=APP_NET_DIAGNOSTIC_MS &&
       (gap_count || error_count || restart_failures)) {
        last_report=now;
        LOGW("串口3汇总：超时=%lu 错误=%lu 码=0x%02lX 重启失败=%lu",
             (unsigned long)gap_count,(unsigned long)error_count,
             (unsigned long)error_bits,(unsigned long)restart_failures);
        gap_count=0; error_count=0; error_bits=0; restart_failures=0;
    }
}
