"""Compile actual arbiter and W25Q driver with a stateful SPI Flash model."""
from pathlib import Path
import os,subprocess
root=Path(__file__).resolve().parents[1];out=root/'tmp/flash-bus-test';out.mkdir(parents=True,exist_ok=True)
header=r'''
#ifndef FLASH_TEST_HAL
#define FLASH_TEST_HAL
#include <stdint.h>
#include <stddef.h>
typedef struct {unsigned port;} GPIO_TypeDef;
typedef struct {int unused;} TIM_HandleTypeDef;
typedef struct {uint32_t CCR4;} Timer;
typedef struct {uint32_t Pin,Mode,Pull,Speed;} GPIO_InitTypeDef;
extern GPIO_TypeDef ga,gb;extern Timer tim3;
#define GPIOA (&ga)
#define GPIOB (&gb)
#define TIM3 (&tim3)
#define GPIO_PIN_SET 1u
#define GPIO_PIN_RESET 0u
#define GPIO_MODE_INPUT 0u
#define GPIO_MODE_OUTPUT_PP 1u
#define GPIO_MODE_AF_PP 2u
#define GPIO_NOPULL 0u
#define GPIO_SPEED_FREQ_HIGH 1u
#define GPIO_SPEED_FREQ_LOW 0u
#define HAL_OK 0
unsigned __get_IPSR(void);unsigned __get_PRIMASK(void);unsigned __get_BASEPRI(void);
uint32_t HAL_GetTick(void);
void HAL_GPIO_WritePin(GPIO_TypeDef*,uint32_t,uint32_t);
void HAL_GPIO_Init(GPIO_TypeDef*,GPIO_InitTypeDef*);
int HAL_SPI_Transmit(void*,uint8_t*,uint16_t,uint32_t);
int HAL_SPI_Receive(void*,uint8_t*,uint16_t,uint32_t);
void fake_enable(int);
#define __HAL_SPI_DISABLE(p) ((void)(p),fake_enable(0))
#define __HAL_SPI_ENABLE(p) ((void)(p),fake_enable(1))
''' + ''.join(f'#define GPIO_PIN_{i} (1u<<{i})\n' for i in range(16))+'#endif\n'
(out/'stm32f1xx_hal.h').write_text(header)
(out/'spi.h').write_text('extern int hspi1;\n')
(out/'FreeRTOS.h').write_text('#include <assert.h>\n#define configASSERT(x) assert(x)\n#define pdTRUE 1\n#define pdMS_TO_TICKS(x) (x)\n#define taskSCHEDULER_RUNNING 2\n')
(out/'task.h').write_text('typedef void* TaskHandle_t;\nTaskHandle_t xTaskGetCurrentTaskHandle(void);\nint xTaskGetSchedulerState(void);\nvoid vTaskDelay(unsigned);\n')
(out/'semphr.h').write_text('typedef void* SemaphoreHandle_t;\nSemaphoreHandle_t xSemaphoreCreateMutex(void);\nint xSemaphoreTake(void*,unsigned);\nint xSemaphoreGive(void*);\n')
code=r'''
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include "app_config.h"
#include "bus_arbiter.h"
#include "w25q64.h"
GPIO_TypeDef ga={0},gb={1};Timer tim3;int hspi1;
static uint8_t mem[8192];
static uint32_t now,address,id=0xef4017;static unsigned cmd,count,selected,enabled,locked,fault,deny,ipsr,primask,basepri;
static unsigned mode[16],level[16],wel,busy,stuck,fail_rx,corrupt;
void *xSemaphoreCreateMutex(void){return (void*)1;}
int xSemaphoreTake(void *m,unsigned ms){(void)m;(void)ms;if(deny)return 0;assert(!locked);locked=1;return 1;}
int xSemaphoreGive(void *m){(void)m;assert(locked);locked=0;return 1;}
void *xTaskGetCurrentTaskHandle(void){return (void*)2;}
int xTaskGetSchedulerState(void){return 2;}
int bsp_is_faulted(void){return fault;}
unsigned __get_IPSR(void){return ipsr;}unsigned __get_PRIMASK(void){return primask;}unsigned __get_BASEPRI(void){return basepri;}
uint32_t HAL_GetTick(void){return now;}
void vTaskDelay(unsigned n){now+=n;if(busy&&!stuck)busy=0;}
void fake_enable(int v){enabled=(unsigned)v;}
void HAL_GPIO_Init(GPIO_TypeDef *p,GPIO_InitTypeDef *g){unsigned i;assert(p==GPIOA);for(i=0;i<16;i++)if(g->Pin&(1u<<i)){if(i==6)assert(g->Mode==GPIO_MODE_INPUT);mode[i]=g->Mode;}}
void HAL_GPIO_WritePin(GPIO_TypeDef *p,uint32_t pins,uint32_t v){unsigned i;assert(p==GPIOA);for(i=0;i<16;i++)if(pins&(1u<<i))level[i]=v;
 if(pins&GPIO_PIN_4){
  if(!v){selected=1;cmd=count=address=0;}
  else if(selected){selected=0;if(cmd==6)wel=1;if(cmd==2 || cmd==0x20){wel=0;busy=1;}}
 }}
int HAL_SPI_Transmit(void *p,uint8_t *b,uint16_t n,uint32_t timeout){unsigned i;(void)p;(void)timeout;assert(locked&&enabled&&selected);
 for(i=0;i<n;i++){
  if(count==0)cmd=b[i];
  else if((cmd==3||cmd==2||cmd==0x20)&&count<=3){address=(address<<8)|b[i];if(count==3&&cmd==0x20){assert(wel&&address%4096==0);memset(mem+address,0xff,4096);}}
  else if(cmd==2){assert(wel&&address<sizeof(mem));mem[address++]&=corrupt?0:b[i];}
  count++;
 }return 0;}
int HAL_SPI_Receive(void *p,uint8_t *b,uint16_t n,uint32_t timeout){unsigned i;(void)p;(void)timeout;assert(locked&&enabled&&selected);if(fail_rx)return 1;
 for(i=0;i<n;i++){
  if(cmd==5)b[i]=(uint8_t)((busy?1:0)|(wel?2:0));
  else if(cmd==0x9f)b[i]=(uint8_t)(id>>(16u-8u*i));
  else {assert(cmd==3&&address<sizeof(mem));b[i]=mem[address++];}
 }return 0;}
static void safe(void){assert(!locked&&!enabled&&level[4]==0&&level[5]==0&&level[7]==0&&mode[6]==GPIO_MODE_INPUT&&tim3.CCR4==0);}
int main(void){
 uint8_t data[40],read[40];memset(mem,0xff,sizeof(mem));memset(data,0xa5,sizeof(data));assert(!spi_bus_init());
 assert(!w25q_init()&&w25q_id()==W25Q_JEDEC_ID);safe();
 assert(w25q_program(250,data,20)==-1);safe();assert(!w25q_program(256,data,40));safe();
 assert(!w25q_read(256,read,40)&&!memcmp(data,read,40));safe();assert(w25q_program(256,data,40)==-1);safe();
 assert(w25q_erase(1)==-1);assert(!w25q_erase(0));safe();assert(w25q_erases()==1);
 corrupt=1;assert(w25q_program(256,data,40)==-1);safe();corrupt=0;
 stuck=busy=1;assert(w25q_read(0,read,1)==-1);assert(now>=APP_FLASH_PROGRAM_MS);safe();stuck=busy=0;
 fail_rx=1;assert(w25q_read(0,read,1)==-1);safe();fail_rx=0;
 deny=1;assert(w25q_read(0,read,1)==-1);deny=0;
 fault=1;assert(spi_bus_take(100)==-1);fault=0;
 ipsr=1;assert(spi_bus_take(100)==-1);ipsr=0;primask=1;assert(spi_bus_take(100)==-1);primask=0;basepri=80;assert(spi_bus_take(100)==-1);basepri=0;
 id=0x123456;assert(w25q_init()==-1);assert(w25q_program(0,data,1)==-1);safe();
 puts("PASS actual W25Q/arbiter: identity, WEL, page/alignment validation, readback, busy timeout, failure cleanup, no MISO output, fault/ISR guards");return 0;
}
'''
(out/'test.c').write_text(code)
env=dict(os.environ);env['PATH']='D:/msys64/ucrt64/bin;'+env['PATH']
args=['D:/msys64/ucrt64/bin/gcc.exe','-std=c99','-Wall','-Wextra','-Werror']
for p in ['tmp/flash-bus-test','App','BSP','Drivers_Device/w25q64']:args+=['-I',p]
args+=['tmp/flash-bus-test/test.c','BSP/bus_arbiter.c','Drivers_Device/w25q64/w25q64.c','-o','tmp/flash-bus-test/test.exe']
subprocess.run(args,cwd=root,env=env,check=True);subprocess.run([str(out/'test.exe')],cwd=root,env=env,check=True)
