#include "bsp_services.h"
#include "app_config.h"
#include "adc.h"
#include "FreeRTOS.h"
#include "task.h"
static uint16_t samples[APP_ADC_DMA_SAMPLES];
static volatile uint32_t vdda;
static volatile uint32_t temperature_raw;
static IWDG_HandleTypeDef watchdog;

static void average(unsigned start)
{
    uint32_t reference=0,temperature=0; unsigned i;
    for (i=0;i<ADC_SAMPLES_PER_CH;i++) {
        temperature+=samples[start+i*ADC_CHANNEL_COUNT];
        reference+=samples[start+i*ADC_CHANNEL_COUNT+1u];
    }
    temperature_raw=temperature/ADC_SAMPLES_PER_CH;
    reference/=ADC_SAMPLES_PER_CH;
    /* F103 calibrated source is NOT established: candidate addresses retained
     * in bsp_pin.h are never dereferenced here. Nominal VREFINT = 1200mV.
     * VDDA[mV] = 1200 * 4095 / ADC_VREFINT. Zero/out-of-range is invalid.
     */
    vdda=(reference && reference<=ADC_FULL_SCALE) ? VREFINT_TYP_MV*ADC_FULL_SCALE/reference:0;
}
void HAL_ADC_ConvHalfCpltCallback(ADC_HandleTypeDef *adc)
{ if(adc->Instance==ADC1) average(0); }
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *adc)
{ if(adc->Instance==ADC1) average(APP_ADC_DMA_SAMPLES/2u); }
void HAL_ADC_ErrorCallback(ADC_HandleTypeDef *adc)
{ if(adc->Instance==ADC1) vdda=0; }
uint32_t vdda_mv_get(void) { return vdda; }
int mcu_temp_get(int32_t *value)
{
    if(value) *value=APP_TEMP_FAULT_VALUE;
    /* No trusted TS_CAL1/2: quality FAULT, never manufacture a normal value.
     * Trusted future one-point formula is T=30+(Vcal30-Vsense)/4.3.
     */
    return -1;
}
int bsp_adc_start(void)
{
    MX_ADC1_Init();
    if(HAL_ADCEx_Calibration_Start(&hadc1)!=HAL_OK) return -1;
    return HAL_ADC_Start_DMA(&hadc1,(uint32_t *)samples,APP_ADC_DMA_SAMPLES)==HAL_OK ? 0:-1;
}

static int wait_rtc(uint32_t mask)
{
    uint32_t start=HAL_GetTick();
    while(!(RTC->CRL&mask)) {
        if((uint32_t)(HAL_GetTick()-start)>=APP_RTC_TIMEOUT_MS) return -1;
        vTaskDelay(1);
    }
    return 0;
}
uint32_t rtc_get(void)
{
    uint32_t a,b,lo;
    do { a=RTC->CNTH; lo=RTC->CNTL; b=RTC->CNTH; } while(a!=b);
    return (a<<16)|lo;
}
int rtc_set(uint32_t seconds)
{
    if(wait_rtc(RTC_CRL_RTOFF)) return -1;
    RTC->CRL|=RTC_CRL_CNF;
    RTC->CNTH=seconds>>16; RTC->CNTL=seconds&0xffffu;
    RTC->CRL&=~RTC_CRL_CNF;
    return wait_rtc(RTC_CRL_RTOFF);
}
int bsp_rtc_start(void)
{
    uint32_t start=HAL_GetTick(),source=RCC_RTCCLKSOURCE_LSE,divider=BSP_LSE_HZ;
    __HAL_RCC_PWR_CLK_ENABLE(); __HAL_RCC_BKP_CLK_ENABLE(); HAL_PWR_EnableBkUpAccess();
    __HAL_RCC_LSE_CONFIG(RCC_LSE_ON);
    while(!__HAL_RCC_GET_FLAG(RCC_FLAG_LSERDY)) {
        if((uint32_t)(HAL_GetTick()-start)>=APP_LSE_TIMEOUT_MS) {
            source=RCC_RTCCLKSOURCE_LSI; divider=BSP_LSI_NOMINAL_HZ;
            LOGW("RTC LSE timeout; LSI fallback (approximate seconds)"); break;
        }
        vTaskDelay(1);
    }
    if(source==RCC_RTCCLKSOURCE_LSI) {
        __HAL_RCC_LSI_ENABLE(); start=HAL_GetTick();
        while(!__HAL_RCC_GET_FLAG(RCC_FLAG_LSIRDY)) {
            if((uint32_t)(HAL_GetTick()-start)>=APP_LSI_TIMEOUT_MS) return -1;
            vTaskDelay(1);
        }
    }
    if((RCC->BDCR&RCC_BDCR_RTCSEL)!=source) {
        __HAL_RCC_BACKUPRESET_FORCE(); __HAL_RCC_BACKUPRESET_RELEASE();
        if(source==RCC_RTCCLKSOURCE_LSE) {
            __HAL_RCC_LSE_CONFIG(RCC_LSE_ON); start=HAL_GetTick();
            while(!__HAL_RCC_GET_FLAG(RCC_FLAG_LSERDY)) {
                if((uint32_t)(HAL_GetTick()-start)>=APP_LSE_TIMEOUT_MS) return -1;
                vTaskDelay(1);
            }
        }
        __HAL_RCC_RTC_CONFIG(source);
    }
    __HAL_RCC_RTC_ENABLE();
    RTC->CRL&=~RTC_CRL_RSF;
    if(wait_rtc(RTC_CRL_RSF)||wait_rtc(RTC_CRL_RTOFF)) return -1;
    RTC->CRL|=RTC_CRL_CNF;
    RTC->PRLH=(divider-1u)>>16; RTC->PRLL=(divider-1u)&0xffffu;
    RTC->CRL&=~RTC_CRL_CNF;
    if(wait_rtc(RTC_CRL_RTOFF)) return -1;
    LOGI("RTC source=%s unix=%lu (set epoch before use)",source==RCC_RTCCLKSOURCE_LSE?"LSE":"LSI",(unsigned long)rtc_get());
    return 0;
}
int bsp_watchdog_start(void)
{
    watchdog.Instance=IWDG; watchdog.Init.Prescaler=IWDG_PRESCALER;
    watchdog.Init.Reload=IWDG_RELOAD;
    return HAL_IWDG_Init(&watchdog)==HAL_OK?0:-1;
}
void bsp_watchdog_feed(void) { HAL_IWDG_Refresh(&watchdog); }
