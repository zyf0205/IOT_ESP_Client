#ifndef SENSOR_H
#define SENSOR_H

#include <stdbool.h>
#include "esp_err.h"

extern bool is_register;

esp_err_t report_task_start(void);

#endif