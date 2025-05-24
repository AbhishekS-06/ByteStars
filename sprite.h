// In sprite.h
#ifndef SPRITE_H // Example include guard, use yours if different
#define SPRITE_H

#include <stdint.h> // Make sure this is included

typedef struct {
  int32_t x;     // x coordinate (top-left corner)
  int32_t y;     // y coordinate (top-left corner)
  int32_t vx,vy; // velocities
  const uint16_t *image; // pointer to image data array (16-bit color)
  uint16_t w;    // width
  uint16_t h;    // height
  uint32_t life; // 0=dead/inactive, 1=alive/active
} Sprite;

// Keep any other definitions or function prototypes already in sprite.h

#endif // SPRITE_H