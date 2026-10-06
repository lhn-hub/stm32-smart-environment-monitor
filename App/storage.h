#ifndef APP_STORAGE_H
#define APP_STORAGE_H
#include "eventlog.h"
#include "kvstore.h"
enum { CFG_TEMP,CFG_DISTANCE,CFG_SMOKE,CFG_AIR,CFG_MODE,CFG_FIELD_COUNT };
enum { EVENT_BOOT=1,EVENT_CONFIG=2,EVENT_MANUAL=3 };
typedef struct {uint32_t ready,errors,dropped,erases,config_seq,pending; } storage_status_t;
/** @brief Create bounded request queue/mutex; before scheduler. */
int storage_setup(void);
/** @brief LogTask init/pump. Sole owner of KV/eventlog/Flash. */
void storage_start(void);
void storage_poll(void);
/** @brief Menu snapshots in short RAM lock; no Flash access. */
int storage_get_fields(int32_t values[CFG_FIELD_COUNT],storage_status_t *status);
/** @brief Enqueue change; RAM/hotload only changes AFTER committed readback. */
int storage_set(unsigned field,int32_t value);
int storage_event(uint32_t type,int32_t parameter,int32_t auxiliary);
int storage_recent(unsigned index,event_record_t *out,unsigned *count);
#endif
