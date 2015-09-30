#include <pebble.h>
#pragma once
  
typedef struct Palette {
  GColor red;
  GColor yellow;
  GColor blue;
  GColor orange;
  GColor green;
  GColor purple;
  GColor all;
  GColor none;
} Palette;

typedef struct TimeSegment {
  GColor color;
  uint16_t value;
  char unit;
} TimeSegment;
