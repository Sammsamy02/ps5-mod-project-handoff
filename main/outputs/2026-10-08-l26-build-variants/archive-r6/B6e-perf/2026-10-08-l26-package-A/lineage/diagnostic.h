#pragma once
#include <stdint.h>
int diagnostic_ready(void);
void diagnostic_event(const char *tag, uint32_t value);
void diagnostic_missing(unsigned index);
int diagnostic_open_retained(void);
int diagnostic_open_data_directory(void);
int diagnostic_fd_event(int fd, const char *tag, int32_t result, int32_t error_number);
int64_t diagnostic_fd_marker(int fd, const char *tag);
int diagnostic_close_retained(int fd);
int diagnostic_open_channel(void);
int diagnostic_channel_fd(void);
int diagnostic_close_channel(void);
