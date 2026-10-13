#ifndef EVENTLOG_H
#define EVENTLOG_H
#include <stdint.h>
#include <stddef.h>
typedef struct {
    uint32_t magic,seq,unix_time,type;
    int32_t parameter,auxiliary;
    uint32_t crc32,commit;
} event_record_t;
/** @brief Scan CRC-valid committed records; never autoformat nonblank unknown data. */
int eventlog_init(void);
/** @brief LogTask only: append32B, rotate physical sectors, commit last. */
int log_append(uint32_t unix_time,uint32_t type,int32_t parameter,int32_t auxiliary);
/** @brief LogTask only: newest first, caller supplies space for n records. */
int log_read_last(size_t n,event_record_t *out,size_t *read_count);
uint32_t eventlog_next_seq(void);
uint32_t eventlog_count(void);
#endif
