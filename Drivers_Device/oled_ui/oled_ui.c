#include "oled_ui.h"
#include "ssd1306.h"
#include "app_config.h"
void ui_row(unsigned row,const char *label,int selected)
{
    int y=APP_UI_ROWS_Y+(int)row*APP_UI_ROW_HEIGHT;
    oled_text(0,y,label,0);
    if(selected)oled_rect(0,y,APP_OLED_WIDTH,APP_UI_ROW_HEIGHT,2,1);
}
void ui_progress(int x,int y,int w,int h,int value,int max)
{
    if(w<2 || h<2)return;
    if(value<0)value=0;
    if(value>max)value=max;
    oled_rect(x,y,w,h,1,0);
    if(max>0)oled_rect(x+1,y+1,value*(w-2)/max,h-2,1,1);
}
void ui_status(int wifi,int mqtt,int armed,int alarm,int warmup)
{
    int flags[]={wifi,mqtt,armed,alarm,warmup};
    const char *labels[] = {"W","M","A","!","P"};
    unsigned i;
    for(i=0;i<5;i++) {
        int x=APP_UI_STATUS_X+(int)i*APP_UI_STATUS_STEP;
        oled_text(x,0,labels[i],0);
        if(flags[i])oled_rect(x,0,7,8,2,1);
        else oled_line(x,8,x+5,8,1);
    }
}
