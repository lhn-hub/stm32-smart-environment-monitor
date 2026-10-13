#ifndef APP_TIME_H
#define APP_TIME_H
#include <stdint.h>
enum { TIME_UNSET, TIME_BUILD, TIME_BACKUP, TIME_GATEWAY };
/* SensorTask owns RTC writes; other tasks only enqueue one validated timestamp. */
void app_time_start(void);
void app_time_poll(void);
int app_time_request(uint32_t unix_seconds);
unsigned app_time_source(void);
uint32_t app_time_build_epoch(const char *date,const char *time);
#endif
