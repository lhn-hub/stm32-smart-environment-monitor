#ifndef DEVICE_MQ_GAS_H
#define DEVICE_MQ_GAS_H
#include "sensor.h"
int mq_init(sensor_id_t id);
int mq_read(sensor_id_t id,sensor_value_t *out);
int mq_ioctl(sensor_id_t id,unsigned command,void *arg);
#endif
