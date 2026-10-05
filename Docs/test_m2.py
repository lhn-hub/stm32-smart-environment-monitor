"""Host tests compile actual device code with bounded HAL/RTOS stubs."""
from pathlib import Path
import os, subprocess
root=Path(__file__).resolve().parents[1]
env=dict(os.environ);env['PATH']='D:/msys64/ucrt64/bin;'+env['PATH']
out=root/'tmp/m2';out.mkdir(exist_ok=True)
def compile_run(name,source):
    (out/(name+'.c')).write_text(source,encoding='utf-8')
    subprocess.run(['D:/msys64/ucrt64/bin/gcc.exe','-std=c99','-Wall','-Wextra','-Werror',
      '-I','BSP','-I','App','-I','Inc','-I','Drivers_Device/ec11','-I','Drivers_Device/oled_ssd1306','-I','App/menu',
      '-I','Drivers_Device/oled_ui','-I','tmp/m2','tmp/m2/'+name+'.c','-o','tmp/m2/'+name+'.exe'],cwd=root,env=env,check=True)
    subprocess.run([str(out/(name+'.exe'))],cwd=root,env=env,check=True)
ec=(root/'Drivers_Device/ec11/ec11.c').read_text(encoding='utf-8')
part=ec[ec.index('void ec11_reset'):ec.index('static uint8_t sample')]
compile_run('encoder',r'''
#include <stdint.h>
#include <assert.h>
#include <stdio.h>
#define APP_EC11_EDGES_PER_DETENT 4
static uint8_t previous;
static int phase;
static uint32_t invalid;
'''+part+r'''
int main(void){
 unsigned n,i;int sum=0;const uint8_t cw[]={1,0,2,3},ccw[]={2,0,1,3};
 ec11_reset(3);
 for(n=0;n<20;n++)for(i=0;i<4;i++){int d=ec11_feed(cw[i]);assert(d>=0);sum+=d;}
 assert(sum==20);sum=0;
 for(n=0;n<20;n++)for(i=0;i<4;i++){int d=ec11_feed(ccw[i]);assert(d<=0);sum+=d;}
 assert(sum==-20);
 ec11_reset(3);sum=0;
 for(n=0;n<20;n++){
  sum+=ec11_feed(1);sum+=ec11_feed(3);sum+=ec11_feed(1);
  sum+=ec11_feed(0);sum+=ec11_feed(2);sum+=ec11_feed(3);
 }
 assert(sum==20);ec11_reset(3);assert(ec11_feed(0)==0);assert(ec11_feed(3)==0);
 puts("PASS EC11 actual decoder: +20/-20, no reverse, bounced edges, invalid two-bit transitions");return 0;
}
''')
prefix=r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define __BSP_PIN_H
#define __I2C_H__
#define DEF_TEMP_ALARM_C_X10 500
#define DEF_INTRUSION_DIST_CM 100
#define DEF_SMOKE_CONFIRM_MS 3000
#define DEF_AIR_CONFIRM_MS 10000
#define MQ_WARMUP_S 150
#define RGB_ARR 999
#define OLED_ADDR 0x78u
#define I2C_MEMADD_SIZE_8BIT 1
#define HAL_OK 0
static int hi2c1,missing;
static uint32_t now,us;
static unsigned writes,recoveries;
uint32_t HAL_GetTick(void){return now;}
uint32_t bsp_micros(void){return us;}
int bsp_i2c_recover(void){recoveries++;return 0;}
static int HAL_I2C_Mem_Write(void *h,unsigned addr,unsigned control,unsigned mode,
 uint8_t *data,unsigned n,unsigned timeout){
 (void)h;(void)mode;(void)data;(void)timeout;assert(addr==0x78u);assert(control==0||control==0x40);
 writes++;us+=(n+2)*23;return missing?-1:0;
}
#include "../../Drivers_Device/oled_ssd1306/ssd1306.c"
'''
compile_run('display',prefix+r'''
int main(void){
 unsigned n;
 assert(oled_init()==0);assert(oled_flush(1)==0);n=writes;
 assert(oled_flush(0)==0 && writes==n);
 oled_pixel(2,2,1);assert(oled_flush(0)==0 && writes==n+5);
 oled_pixel(-1,-1,1);oled_pixel(128,64,1);n=writes;assert(oled_flush(0)==0 && writes==n);
 missing=1;oled_pixel(0,0,1);assert(oled_flush(0)==-1 && !stats.online);
 n=writes;assert(oled_flush(0)==-1 && writes==n);
 missing=0;now+=APP_OLED_RETRY_MS;assert(oled_flush(0)==0);
 assert(stats.online && stats.failures==0 && recoveries==1 && stats.recoveries==1);
 oled_clear();oled_text(0,0,"sms-hub 6x8",0);oled_text(0,12,"8x16",1);
 for(n=0;n<7;n++)oled_hanzi(n*16,36,n);
 {FILE *f=fopen("tmp/m2/fonts.pbm","wb");unsigned x,y;assert(f);fprintf(f,"P1\n128 64\n");
 for(y=0;y<64;y++){for(x=0;x<128;x++)fprintf(f,"%u ",(pixels[y/8*128+x]>>(y&7))&1);fputc('\n',f);}fclose(f);}
 puts("PASS OLED actual driver: 1KB buffer, dirty page/no-op refresh, clipping, unplug/retry/recover/reset consecutive failures");return 0;
}
''')
# Minimal headers for the menu's read-only RTOS diagnostics.
(out/'FreeRTOS.h').write_text('')
(out/'task.h').write_text('static unsigned uxTaskGetStackHighWaterMark(void *p){(void)p;return 200;}\n')
compile_run('menu',prefix+r'''
#include "bsp_services.h"
static int pending,beeps;
static unsigned colors;
void ec11_start(void){}
int ec11_poll(void){int n=pending;pending=0;return n;}
uint32_t ec11_overflows(void){return 0;}
uint8_t ec11_levels(void){return 3;}
uint32_t ec11_irq_count(void){return 0;}
uint32_t ec11_invalid_count(void){return 0;}
uint32_t rtc_get(void){return 3661;}
uint32_t vdda_mv_get(void){return 3300;}
int beep_ms(uint16_t ms){(void)ms;beeps++;return 0;}
int rgb_set(uint16_t r,uint16_t g,uint16_t b){colors=r+g+b;return 0;}
void i2c_scan(void){}
void bsp_log(const char *level,const char *format,...){(void)level;(void)format;}
#include "../../Drivers_Device/oled_ui/oled_ui.c"
#include "../../App/menu/menu.c"
int main(void){
 unsigned i;key_event_t yes={KEY_CONFIRM,KEY_CLICK,0},back={KEY_BACK,KEY_CLICK,0},hold={KEY_CONFIRM,KEY_LONG,0};
 menu_init();assert(current()==HOME);menu_key(&yes);assert(current()==ROOT);
 for(i=1;i<8;i++){selected[ROOT]=i;menu_key(&yes);assert(depth==2);menu_poll();menu_key(&back);assert(current()==ROOT && depth==1);}
 selected[ROOT]=2;menu_key(&yes);menu_key(&yes);assert(editing);
 pending=1;menu_poll();assert(values[0]==51);pending=-1;menu_poll();assert(values[0]==50);
 menu_key(&hold);pending=1;menu_poll();assert(values[0]==60);
 pending=1000;menu_poll();assert(values[0]==APP_EDIT_MAX_TEMP);
 menu_key(&back);selected[ROOT]=3;menu_key(&yes);menu_key(&yes);assert(beeps==1);
 selected[MANUAL]=1;menu_key(&yes);assert(colors==999);selected[MANUAL]=4;menu_key(&yes);assert(colors==0);
 for(i=0;i<PAGE_COUNT;i++){
  char path[64];FILE *f;unsigned x,y;stack[depth]=(page_id)i;oled_clear();pages[i].render();
  snprintf(path,sizeof(path),"tmp/m2/page-%u.pbm",i);f=fopen(path,"wb");assert(f);fprintf(f,"P1\n128 64\n");
  for(y=0;y<64;y++){for(x=0;x<128;x++)fprintf(f,"%u ",(pixels[y/8*128+x]>>(y&7))&1);fputc('\n',f);}fclose(f);
 }
 puts("PASS actual menu: every page reachable/back, depth3, edit/accelerate/clamp, manual beep/RGB, all pages rendered");return 0;
}
''')
from PIL import Image,ImageOps,ImageDraw
paths=[out/'fonts.pbm']+sorted(out.glob('page-*.pbm'))
sheet=Image.new('RGB',(640, len(paths)*280),'#222222');draw=ImageDraw.Draw(sheet)
for i,p in enumerate(paths):
    im=Image.open(p).convert('L');im=ImageOps.invert(im).resize((512,256),Image.Resampling.NEAREST)
    sheet.paste(im,(0,i*280+18));draw.text((0,i*280),p.stem,fill='white')
sheet.save(out/'preview.png')
