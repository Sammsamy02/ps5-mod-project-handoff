#pragma once
#include <stdint.h>
int diagnostic_ready(void);
int diagnostic_open_channel(void);
void diagnostic_close_channel(void);
void diagnostic_event(const char *, uint32_t);
