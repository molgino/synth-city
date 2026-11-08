// Ersatz fuer den Web-Build (Emscripten hat keine Linux-Kernel-Header): nur was main.cpp braucht, im Browser nie aktiv
#pragma once
#include <sys/time.h>
struct input_event { struct timeval time; unsigned short type, code; int value; };
#define EV_KEY 0x01
#define KEY_VOLUMEDOWN 114
#define KEY_VOLUMEUP 115
#define KEY_POWER 116
#define EVIOCGNAME(len) (0x80004506 | ((len) << 16))
