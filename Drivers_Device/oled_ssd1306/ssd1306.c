#include "ssd1306.h"
#include "app_config.h"
#include "bsp_services.h"
#include "bsp_timer_us.h"
#include "i2c.h"
#include <string.h>
#include <stdlib.h>
#include "fonts.h"
static uint8_t pixels[APP_OLED_WIDTH*APP_OLED_PAGES];
static uint8_t dirty;
static oled_stats_t stats;
static uint32_t retry_at;
static int transfer(uint16_t control,const uint8_t *data,uint16_t n)
{
    uint32_t start,result;
    /* Long polling printf at higher priority can spend this entire8ms HAL
     * budget while I2C waits for its next byte. Reuse console mutex per chunk:
     * pending log tasks lend priority, interrupts remain enabled throughout. */
    if(bsp_log_quiet_begin(APP_LOG_LOCK_MS))return OLED_DEFERRED;
    start=bsp_micros();
    result=(uint32_t)HAL_I2C_Mem_Write(&hi2c1,OLED_ADDR,control,I2C_MEMADD_SIZE_8BIT,
        (uint8_t*)data,n,APP_OLED_TIMEOUT_MS);
    if(result!=(uint32_t)HAL_OK){
        stats.last_hal_status=result;stats.last_hal_error=hi2c1.ErrorCode;
        stats.last_failed_us=bsp_micros()-start;
        bsp_log_quiet_end();return -1;
    }
    bsp_log_quiet_end();
    return 0;
}
static int command(const uint8_t *data,uint16_t n){return transfer(0,data,n);}
static int failed(void)
{
    ++stats.failures; stats.online=0;
    retry_at=HAL_GetTick()+APP_OLED_RETRY_MS; dirty=0xff;
    return -1;
}
int oled_init(void)
{
    int result;
    /* SSD1306 controller commands, not GPIO/period/threshold configuration. */
    static const uint8_t sequence[]={0xae,0xd5,0x80,0xa8,0x3f,0xd3,0,0x40,0x8d,0x14,
        0x20,0x02,0xa1,0xc8,0xda,0x12,0x81,0x7f,0xd9,0xf1,0xdb,0x40,0xa4,0xa6,0xaf};
    result=command(sequence,sizeof(sequence));
    if(result)return result==OLED_DEFERRED?result:failed();
    stats.online=1; dirty=0xff; return 0;
}
int oled_flush(int full)
{
    uint32_t begin=bsp_micros(); unsigned p,x;int result;
    if(!stats.online) {
        if((int32_t)(HAL_GetTick()-retry_at)<0) return -1;
        ++stats.recoveries;
        if(bsp_i2c_recover()) return failed();
        result=oled_init();if(result)return result;
        full=1;
    }
    if(full) dirty=0xff;
    for(p=0;p<APP_OLED_PAGES;p++) if(dirty&(1u<<p)) {
        uint8_t cmd[]={ (uint8_t)(0xb0u+p),0x00,0x10 };
        result=command(cmd,sizeof(cmd));
        if(result)return result==OLED_DEFERRED?result:failed();
        for(x=0;x<APP_OLED_WIDTH;x+=APP_OLED_CHUNK){
            result=transfer(0x40,&pixels[p*APP_OLED_WIDTH+x],APP_OLED_CHUNK);
            if(result)return result==OLED_DEFERRED?result:failed();
        }
        dirty&=(uint8_t)~(1u<<p);
    }
    stats.failures=0; ++stats.frames;
    stats.last_us=bsp_micros()-begin;
    if(stats.last_us>stats.max_us) stats.max_us=stats.last_us;
    return 0;
}
void oled_clear(void) { memset(pixels,0,sizeof(pixels)); dirty=0xff; }
void oled_pixel(int x,int y,unsigned color)
{
    unsigned pos; uint8_t old,mask;
    if(x<0 || y<0 || x>=(int)APP_OLED_WIDTH || y>=(int)APP_OLED_HEIGHT) return;
    pos=(unsigned)y/8u*APP_OLED_WIDTH+(unsigned)x; mask=(uint8_t)(1u<<(y&7)); old=pixels[pos];
    if(color==2) pixels[pos]^=mask; else if(color) pixels[pos]|=mask; else pixels[pos]&=(uint8_t)~mask;
    if(old!=pixels[pos]) dirty|=(uint8_t)(1u<<(y/8));
}
void oled_line(int x0,int y0,int x1,int y1,unsigned color)
{
    int dx=abs(x1-x0),sx=x0<x1?1:-1,dy=-abs(y1-y0),sy=y0<y1?1:-1,err=dx+dy;
    for(;;) { int e; oled_pixel(x0,y0,color); if(x0==x1 && y0==y1)break; e=2*err;
        if(e>=dy){err+=dy;x0+=sx;} if(e<=dx){err+=dx;y0+=sy;} }
}
void oled_rect(int x,int y,int w,int h,unsigned color,int filled)
{
    int a,b;
    for(b=0;b<h;b++)for(a=0;a<w;a++)if(filled || !a || !b || a==w-1 || b==h-1)oled_pixel(x+a,y+b,color);
}
static void bitmap(int x,int y,const uint8_t *data,unsigned w,unsigned h)
{
    unsigned a,b;
    for(b=0;b<h;b++)for(a=0;a<w;a++)oled_pixel(x+(int)a,y+(int)b,(data[(b/8)*w+a]>>(b&7))&1u);
}
void oled_text(int x,int y,const char *s,unsigned large)
{
    for(;*s && x<(int)APP_OLED_WIDTH;s++) {
        unsigned c=(unsigned char)*s; if(c<32 || c>126)c='?';
        bitmap(x,y,large?font8[c-32]:font6[c-32],large?8:6,large?16:8);x+=large?8:6;
    }
}
void oled_hanzi(int x,int y,unsigned index)
{ if(index<sizeof(font_cn)/sizeof(font_cn[0]))bitmap(x,y,font_cn[index],16,16); }
const oled_stats_t *oled_stats(void) { return &stats; }
