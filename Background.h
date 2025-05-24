// Background.h
// based on your original :contentReference[oaicite:0]{index=0}

#ifndef __BACKGROUND_H__
#define __BACKGROUND_H__

#include <cstdint>
#include <stdint.h>

#define NUM_FRAMES 2

extern uint8_t currentFrame;

extern const unsigned short* holtFrames[NUM_FRAMES];
extern const unsigned short* speightFrames[NUM_FRAMES];
extern const unsigned short* yerrFrames[NUM_FRAMES];
extern const unsigned short* valvFrames[NUM_FRAMES];

void DrawHolt   (int32_t x, int32_t y);
void DrawSpeight(int32_t x, int32_t y);
void DrawYerr   (int32_t x, int32_t y);
void DrawValv   (int32_t x, int32_t y);

void DrawTitle(uint16_t lang);
void DrawCharSel(uint16_t selected);

void detectPixels(const uint16_t* sprite, uint16_t x, uint16_t y, uint32_t w, uint32_t h, uint32_t pix);
void eraseOnBackground(const uint16_t* back, uint16_t x, uint16_t y, uint32_t w, uint32_t h);
void eraseOnSprite(int32_t x, int32_t y, int32_t w, int32_t h);
void Fill(int32_t x, int32_t y, uint16_t w, uint16_t h);

void FillProjectile(int32_t x, int32_t y,int32_t w,int32_t h);
void eraseOnProjectile(int32_t x, int32_t y,int32_t w,int32_t h);
void DrawProjectileWithTransparency(int32_t x, int32_t y, const uint16_t* image, uint16_t w, uint16_t h);

void DrawSpriteWithTransparency(int x, int y, const uint16_t* image, int w, int h);
void DrawGO(uint16_t sel);
#endif
