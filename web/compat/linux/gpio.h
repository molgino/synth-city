// Ersatz fuer den Web-Build (siehe input.h)
#pragma once
#include <stdint.h>
struct gpiochip_info { char name[32], label[32]; uint32_t lines; };
struct gpiohandle_request { uint32_t lineoffsets[64], flags; uint8_t default_values[64]; char consumer_label[32]; uint32_t lines; int fd; };
struct gpiohandle_data { uint8_t values[64]; };
#define GPIOHANDLE_REQUEST_INPUT 1
#define GPIO_GET_CHIPINFO_IOCTL 0x8044b401
#define GPIO_GET_LINEHANDLE_IOCTL 0xc16cb403
#define GPIOHANDLE_GET_LINE_VALUES_IOCTL 0xc040b408
