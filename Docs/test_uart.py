"""Compile the actual DMA drain/parser function with host HAL boundary stubs."""
from pathlib import Path
import os
import subprocess

root = Path(__file__).resolve().parents[1]
source = (root / 'BSP/bsp_bus.c').read_text(encoding='utf-8')
start = source.index('void bsp_net_poll(void)')
brace = source.index('{', start)
depth = 1
end = brace + 1
while depth:
    depth += (source[end] == '{') - (source[end] == '}')
    end += 1
function = source[start:end]
prefix = r'''
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
#define __BSP_PIN_H
#include "app_config.h"
#define ESP_RX_BUF_SIZE 512u
#define HAL_UART_STATE_READY 0
#define HAL_OK 0
#define UART_IT_IDLE 0
static unsigned warnings;
#define LOGW(...) (++warnings)
#define LOGE(...) ((void)0)
#define taskENTER_CRITICAL() ((void)0)
#define taskEXIT_CRITICAL() ((void)0)
static struct { int gState; void *hdmarx; } huart3;
static uint8_t rx[ESP_RX_BUF_SIZE],discard;
static char line[APP_NET_LINE_SIZE],received[APP_NET_LINE_SIZE];
static size_t read_pos,line_len,write_cursor;
static uint32_t last_poll,tx_started,uart_errors,now;
static uint32_t uart_error_bits,gap_count,error_count,error_bits,restart_failures,last_report;
static unsigned delivered,aborted;
#define __HAL_DMA_GET_COUNTER(x) ((void)(x),sizeof(rx)-write_cursor)
#define __HAL_UART_ENABLE_IT(x,y) ((void)(x),(void)(y))
static uint32_t HAL_GetTick(void) { return now; }
static void HAL_UART_AbortTransmit(void *p) { (void)p;aborted++;huart3.gState=0; }
static void HAL_UART_AbortReceive(void *p) { (void)p; }
static int HAL_UART_Receive_DMA(void *p,void *b,size_t n)
{ (void)p;(void)b;(void)n;return 0; }
static void net_on_rx_line(const char *p,size_t n)
{ memcpy(received,p,n);received[n]=0;delivered++; }
'''
suffix = r'''
static void feed(const char *p)
{
 while(*p) { rx[write_cursor]=(uint8_t)*p++;write_cursor=(write_cursor+1)%sizeof(rx); }
 now++;bsp_net_poll();
}
int main(void)
{
 unsigned i;
 feed("hel");assert(!delivered);feed("lo\r\n");assert(delivered==1 && !strcmp(received,"hello"));
 feed("a\nb\n");assert(delivered==3 && !strcmp(received,"b"));
 read_pos=write_cursor=510;feed("wrap\n");assert(!strcmp(received,"wrap"));
 for(i=0;i<APP_NET_LINE_SIZE+2;i++)feed("x");
 i=delivered;feed("\n");assert(delivered==i);feed("ok\n");assert(!strcmp(received,"ok"));
 now+=APP_NET_RING_GUARD_MS+1;bsp_net_poll();i=delivered;
 feed("bad\n");assert(delivered==i);feed("good\n");assert(!strcmp(received,"good"));
 huart3.gState=1;tx_started=now;now+=APP_NET_TX_TIMEOUT_MS;bsp_net_poll();assert(aborted==1);
 /* Sustained hardware errors recover every poll, but report at most per5s. */
 warnings=0;last_report=now;gap_count=error_count=error_bits=restart_failures=0;
 for(i=0;i<200;i++) {
   uart_errors=1;uart_error_bits=4;now+=10;write_cursor=0;bsp_net_poll();
 }
 assert(warnings==0 && error_count==200 && error_bits==4);
 now+=APP_NET_DIAGNOSTIC_MS;bsp_net_poll();assert(warnings==1);
 assert(error_count==0 && error_bits==0);
 /* A reset ring must not drain bytes using its previous write index. */
 delivered=0;write_cursor=5;memcpy(rx,"old\n!",5);uart_errors=1;
 now++;bsp_net_poll();assert(delivered==0 && read_pos==0);
 write_cursor=0;feed("discard\n");feed("fresh\n");assert(!strcmp(received,"fresh"));
 puts("PASS: 200 consecutive errors do not flood logs; error bitmap retained; DMA restart drops stale index");
 puts("PASS actual USART3 drain: fragments, CRLF, multiline, DMA wrap, overflow resync, gap guard, TX timeout");
 return 0;
}
'''
# Raw Python strings above intentionally contain C escapes.
suffix = suffix.replace('\\\\', '\\')
out = root / 'tmp/test_uart.c'
out.write_text(prefix + function + suffix, encoding='utf-8')
env = dict(os.environ)
env['PATH'] = 'D:/msys64/ucrt64/bin;' + env['PATH']
exe = root / 'tmp/test_uart.exe'
subprocess.run(['D:/msys64/ucrt64/bin/gcc.exe','-std=c99','-Wall','-Wextra','-Werror',
                '-I','App','-I','BSP','tmp/test_uart.c','-o','tmp/test_uart.exe'],check=True,env=env,cwd=root)
subprocess.run([str(exe)],check=True,env=env)
