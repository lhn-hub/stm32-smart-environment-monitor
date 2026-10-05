#include "sensor_filter.h"
static void add(sensor_filter_t *f,int32_t v)
{ f->values[f->next]=v; f->next=(f->next+1u)%APP_FILTER_WINDOW; if(f->count<APP_FILTER_WINDOW)++f->count; }
int32_t sensor_median(sensor_filter_t *f,int32_t v)
{
    int32_t a[APP_FILTER_WINDOW]; unsigned i,j;
    add(f,v);
    for(i=0;i<f->count;i++){a[i]=f->values[i];for(j=i;j>0 && a[j]<a[j-1];j--){int32_t t=a[j];a[j]=a[j-1];a[j-1]=t;}}
    return a[f->count/2u];
}
int32_t sensor_mean(sensor_filter_t *f,int32_t v)
{ int32_t sum=0;unsigned i;add(f,v);for(i=0;i<f->count;i++)sum+=f->values[i];return sum/(int32_t)f->count; }
void sensor_switch_init(sensor_switch_t *s,int v,uint32_t now)
{s->candidate=s->stable=(uint8_t)(v!=0);s->since=now;}
int sensor_switch_update(sensor_switch_t *s,int raw,uint32_t now,uint32_t on_ms,uint32_t off_ms)
{
    uint8_t v=(uint8_t)(raw!=0);
    if(v!=s->candidate){s->candidate=v;s->since=now;}
    if(v!=s->stable && (uint32_t)(now-s->since)>=(v?on_ms:off_ms))s->stable=v;
    return s->stable;
}
