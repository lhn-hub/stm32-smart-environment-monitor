"""Compile actual M5 drivers and controls against deterministic peripheral boundaries."""
from pathlib import Path
import os
import subprocess

root = Path(__file__).resolve().parents[1]
out = root / "tmp/m5-test"
out.mkdir(parents=True, exist_ok=True)
env = dict(os.environ)
env["PATH"] = "D:/msys64/ucrt64/bin;" + env["PATH"]
hal = r"""
#ifndef M5_HAL
#define M5_HAL
#include <stdint.h>
typedef struct {uint32_t CCR1,CCR4,BDTR;} Timer;
typedef struct {Timer *Instance;} TIM_HandleTypeDef;
extern Timer tim1,tim3;
#define TIM1 (&tim1)
#define TIM3 (&tim3)
#define TIM_CHANNEL_1 1
#define TIM_CHANNEL_4 4
#define TIM_BDTR_MOE 0x8000u
#define HAL_OK 0
#define __HAL_TIM_SET_COMPARE(t,c,v) ((void)(c),(t)->Instance->CCR1=(v))
uint32_t HAL_GetTick(void);
unsigned __get_IPSR(void);
int HAL_TIM_PWM_Start(TIM_HandleTypeDef *,unsigned);
#endif
"""
(out / "stm32f1xx_hal.h").write_text(hal)
(out / "tim.h").write_text('#include "stm32f1xx_hal.h"\nextern TIM_HandleTypeDef htim1,htim3;\nvoid MX_TIM1_Init(void);\n')
(out / "FreeRTOS.h").write_text('#include <assert.h>\n#define pdMS_TO_TICKS(x) (x)\n#define taskENTER_CRITICAL() ((void)0)\n#define taskEXIT_CRITICAL() ((void)0)\n')
(out / "task.h").write_text("void vTaskDelay(unsigned);\n")
test = r"""
#include <assert.h>
#include <stdio.h>
#include "app_config.h"
#include "sg90.h"
#include "tb6612_fan.h"
Timer tim1,tim3;TIM_HandleTypeDef htim1={&tim1},htim3={&tim3};
static uint32_t now;static unsigned fault,irq,busy,held,starts,stops,waits;
uint32_t HAL_GetTick(void){return now;}
unsigned __get_IPSR(void){return irq;}
int bsp_is_faulted(void){return fault;}
void MX_TIM1_Init(void){tim1.CCR1=SERVO_CCR_MIN;}
int HAL_TIM_PWM_Start(TIM_HandleTypeDef *t,unsigned c){
 if(t==&htim1){assert(c==SERVO_CH);t->Instance->BDTR|=SERVO_MOE_MASK;}
 else assert(t==&htim3&&c==FAN_CH&&tim3.CCR4==0);
 return 0;
}
int fan_bus_take(uint32_t ms){assert(ms<=10);if(busy){++waits;return -1;}assert(!held&&tim3.CCR4==0);held=1;++starts;return 0;}
void fan_bus_give(void){assert(held);tim3.CCR4=0;held=0;++stops;}
static void tick(unsigned ms){now+=ms;servo_poll(now);fan_poll(now);}
int main(void){
 unsigned i,previous;
 assert(!servo_init()&&!fan_init());assert(tim1.CCR1==500&&(tim1.BDTR&SERVO_MOE_MASK)&&!tim3.CCR4);
 assert(servo_write(181)==-1&&fan_set(101)==-1);
 assert(!servo_write(180));tick(49);assert(!servo_angle());tick(1);assert(servo_angle()==1);
 for(i=1;i<90;i++){tick(50);}assert(servo_angle()==90&&tim1.CCR1==1500);
 for(i=90;i<180;i++){tick(50);}assert(servo_angle()==180&&tim1.CCR1==2500);
 assert(!servo_write(0));tick(1000);assert(servo_angle()==179); /* late callback must not jump */
 for(i=179;i>0;i--){tick(50);}assert(!servo_angle()&&tim1.CCR1==500);
 now=UINT32_MAX-24;servo_poll(now);assert(!servo_write(1));tick(49);assert(!servo_angle());tick(1);assert(servo_angle()==1);
 busy=1;assert(!fan_set(100));tick(10);assert(waits==1&&!held&&!tim3.CCR4);
 assert(!fan_set(0));busy=0;tick(10);assert(!held&&starts==0);
 assert(!fan_set(100));tick(10);assert(held&&tim3.CCR4==0);
 for(i=0;i<100;i++){previous=tim3.CCR4;tick(10);assert(tim3.CCR4>=previous&&tim3.CCR4-previous<=36);}
 assert(fan_percent()==100&&tim3.CCR4==3600);
 assert(!fan_set(45));tick(10);tick(500);assert(tim3.CCR4==2610);tick(500);assert(fan_percent()==45&&tim3.CCR4==1620);
 assert(!fan_set(0));assert(!tim3.CCR4);tick(10);assert(!held&&stops==1&&!fan_percent());
 now=UINT32_MAX-20;assert(!fan_set(60));tick(10);tick(500);assert(tim3.CCR4==1080);tick(500);assert(fan_percent()==60);
 fault=1;tim1.BDTR=0;tim3.CCR4=0;assert(servo_write(90)==-1&&fan_set(100)==-1);tick(10);
 assert(!held&&!tim3.CCR4&&!tim1.BDTR);fault=0;irq=1;assert(servo_write(10)==-1&&fan_set(10)==-1);
 puts("PASS actual SG90/fan: endpoints/MOE, 50ms/degree/no catchup, 1s ramps/re-target/stop, contention cancellation, tick wrap, fault/ISR");
 return 0;
}
"""
(out / "test.c").write_text(test)
includes = ["tmp/m5-test", "BSP", "App", "Drivers_Device/sg90", "Drivers_Device/tb6612_fan"]
args = ["D:/msys64/ucrt64/bin/gcc.exe", "-std=c99", "-Wall", "-Wextra", "-Werror"]
for directory in includes:
    args += ["-I", directory]
subprocess.run(args + ["tmp/m5-test/test.c", "Drivers_Device/sg90/sg90.c",
                      "Drivers_Device/tb6612_fan/tb6612_fan.c", "-o", "tmp/m5-test/test.exe"],
               cwd=root, env=env, check=True)
subprocess.run([str(out / "test.exe")], cwd=root, env=env, check=True)
# controls.c's existing single-TU simulation supplies its own HAL/RTOS boundary.
control_includes = ["BSP", "App", "Inc", "Drivers_Device", "Middleware/FreeRTOS/include",
                    "Drivers_Device/dht11", "Drivers_Device/cs100a",
                    "Drivers_Device/sg90", "Drivers_Device/tb6612_fan",
                    "Drivers/STM32F1xx_HAL_Driver/Inc", "Drivers/CMSIS/Include",
                    "Drivers/CMSIS/Device/ST/STM32F1xx/Include"]
args = ["D:/msys64/ucrt64/bin/gcc.exe", "-std=c99", "-Wall", "-Wextra", "-Werror"]
for directory in control_includes:
    args += ["-I", directory]
subprocess.run(args + ["Docs/test_controls.c", "-o", "tmp/m5-test/controls.exe"],
               cwd=root, env=env, check=True)
subprocess.run([str(out / "controls.exe")], cwd=root, env=env, check=True)
bench = r"""
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "app_config.h"
#include "m5_bench.h"
#include "tb6612_fan.h"
#include "storage.h"
#include "bus_arbiter.h"
#include "w25q64.h"
Timer tim1,tim3;TIM_HandleTypeDef htim3={&tim3};
static uint32_t now;
static int fan_held,spi_held,pending,wrong_id;
static event_record_t recent={.seq=1},queued;
static bus_status_t stats;
static unsigned reports,reported_pass,reported_errors,reported_id_errors,probes;
static unsigned long reported_spi_waits,reported_fan_waits;
uint32_t HAL_GetTick(void){return now;}
unsigned __get_IPSR(void){return 0;}
int bsp_is_faulted(void){return 0;}
int HAL_TIM_PWM_Start(TIM_HandleTypeDef *t,unsigned c){(void)t;(void)c;return 0;}
int fan_bus_take(uint32_t ms){
 (void)ms;if(spi_held){++stats.fan_waits;return -1;}
 assert(!fan_held);fan_held=1;stats.mode=BUS_FAN;return 0;
}
void fan_bus_give(void){assert(fan_held);tim3.CCR4=0;fan_held=0;stats.mode=BUS_IDLE;}
void bus_status(bus_status_t *s){*s=stats;}
int storage_recent(unsigned index,event_record_t *event,unsigned *count){assert(!index);*event=recent;*count=1;return 0;}
int storage_event(uint32_t t,int32_t p,int32_t a){
 if(pending)return -1;
 queued.type=t;queued.parameter=p;queued.auxiliary=a;pending=1;return 0;
}
int w25q_probe(uint32_t *id){assert(!fan_held&&!spi_held);++probes;*id=wrong_id?0:W25Q_JEDEC_ID;return 0;}
void bsp_log(const char *level,const char *fmt,...){
 va_list ap;unsigned mode;(void)level;(void)fmt;va_start(ap,fmt);
 mode=va_arg(ap,unsigned);reported_pass=va_arg(ap,unsigned);
 reported_errors=va_arg(ap,unsigned);reported_id_errors=va_arg(ap,unsigned);
 reported_spi_waits=va_arg(ap,unsigned long);reported_fan_waits=va_arg(ap,unsigned long);
 va_end(ap);++reports;
 printf("BENCH MODEL mode=%u pass=%u errors=%u jedec_errors=%u spi_waits=%u fan_waits=%u\n",
  mode,reported_pass,reported_errors,reported_id_errors,(unsigned)reported_spi_waits,(unsigned)reported_fan_waits);
}
void vTaskDelay(unsigned ms){unsigned i;assert(spi_held);for(i=0;i<ms;i+=10){now+=10;fan_poll(now);}}
static void log_poll(void){
 if(!pending)return;
 if(fan_held){++stats.spi_waits;return;}
 spi_held=1;stats.mode=BUS_SPI;m5_bench_storage_hook((unsigned)queued.auxiliary);
 queued.seq=recent.seq+1;recent=queued;pending=0;spi_held=0;stats.mode=BUS_IDLE;
}
static void step(void){now+=10;fan_poll(now);m5_bench_poll();log_poll();}
static void run(unsigned mode){
 unsigned i,initial=reports;m5_bench_request=mode;
 for(i=0;i<20000 && reports==initial;i++){step();}
 assert(reports==initial+1&&!fan_held&&!tim3.CCR4);
}
int main(void){
 assert(!fan_init());
 run(1);assert(reported_pass==100&&!reported_errors&&!reported_id_errors&&reported_spi_waits==100&&!reported_fan_waits&&probes==100);
 run(2);assert(reported_pass==100&&!reported_errors&&!reported_id_errors&&reported_fan_waits>=100&&!reported_spi_waits&&probes==200);
 wrong_id=1;run(1);assert(!reported_pass&&reported_errors==1&&reported_id_errors==1);wrong_id=0;
 m5_bench_request=1;step();step();m5_bench_request=3;step();assert(reported_errors==1&&!fan_held&&!tim3.CCR4);
 puts("PASS actual M5 bench/fan cooperative model: two100-cycle commands, required contention/event-seq/new JEDEC, fail-stop and abort");
 return 0;
}
"""
(out / "bench.c").write_text(bench)
args = ["D:/msys64/ucrt64/bin/gcc.exe", "-std=c99", "-Wall", "-Wextra", "-Werror", "-DAPP_M5_STRESS_ENABLED=1"]
for directory in includes + ["Drivers_Device/eventlog", "Drivers_Device/kvstore", "Drivers_Device/w25q64"]:
    args += ["-I", directory]
subprocess.run(args + ["tmp/m5-test/bench.c", "App/m5_bench.c",
                      "Drivers_Device/tb6612_fan/tb6612_fan.c", "-o", "tmp/m5-test/bench.exe"],
               cwd=root, env=env, check=True)
subprocess.run([str(out / "bench.exe")], cwd=root, env=env, check=True)
board = (root / "BSP/bsp_board.c").read_text(encoding="utf-8")
safe_functions = board[board.index("static volatile uint32_t fault_latched;"):board.index("void bsp_board_init")]
safe = r"""
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
typedef struct {uint32_t CCR1,CCR2,CCR3,CCR4,BDTR;} Timer;
static Timer a,b,c;
static struct {uint32_t CR1;} spi;
#define SPI1 (&spi)
#define SPI_CR1_SPE 64u
#define TIM1 (&a)
#define TIM2 (&b)
#define TIM3 (&c)
#define __HAL_RCC_GPIOA_CLK_ENABLE() ((void)0)
#define __HAL_RCC_GPIOB_CLK_ENABLE() ((void)0)
#define GPIO_PIN_RESET 0
#define GPIO_PIN_SET 1
#define SERVO_CCR_MIN 500
#define SERVO_MOE_MASK 0x8000u
#define TB_STBY_PORT 0
#define TB_STBY_PIN 4
#define TB_BIN1_PORT 1
#define TB_BIN1_PIN 7
#define TB_BIN2_PORT 2
#define TB_BIN2_PIN 13
#define TB_SAFE_LEVEL 0
#define FAN_SAFE_CCR 0
#define RGB_R_PORT 4
#define RGB_R_PIN 1
#define RGB_G_PIN 2
#define RGB_B_PIN 4
#define RGB_OFF_LEVEL 1
static unsigned levels[5];
static void HAL_GPIO_WritePin(unsigned p,unsigned pin,unsigned value){(void)pin;levels[p]=value;}
void bsp_safe_state(void);
""" + safe_functions + r"""
int main(void){
 unsigned i;for(i=0;i<5;i++)levels[i]=1;
 a.BDTR=SERVO_MOE_MASK;a.CCR1=2500;b.CCR2=b.CCR3=b.CCR4=999;c.CCR4=3600;spi.CR1=SPI_CR1_SPE;
 bsp_emergency_stop();
 assert(bsp_is_faulted()&&!spi.CR1&&!a.BDTR&&a.CCR1==500&&!c.CCR4&&!b.CCR2&&!b.CCR3&&!b.CCR4);
 for(i=0;i<3;i++)assert(!levels[i]);
 assert(levels[3]==1); /* Removed buzzer pin is never driven. */
 assert(levels[4]==RGB_OFF_LEVEL);
 puts("PASS actual BSP emergency: latched fault, SPI disabled, STBY/direction/fan/RGB safe, removed buzzer untouched, TIM1 MOE disabled");
 return 0;
}
"""
(out / "safe.c").write_text(safe)
subprocess.run(["D:/msys64/ucrt64/bin/gcc.exe", "-std=c99", "-Wall", "-Wextra", "-Werror",
                "tmp/m5-test/safe.c", "-o", "tmp/m5-test/safe.exe"], cwd=root, env=env, check=True)
subprocess.run([str(out / "safe.exe")], cwd=root, env=env, check=True)
