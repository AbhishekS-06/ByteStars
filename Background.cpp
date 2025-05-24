/*
 * Background.cpp
 *
 *  Created on: Nov 5, 2023
 *      Author:
 */
// #include <cstdint>
#include <cstdint>
#include <ti/devices/msp/msp.h>
#include "../inc/LaunchPad.h"
#include "Background.h"
#include "images/byteStars/images.h"
#include "SpriteLoader.h"
#include "..//ST7735_SDC.h"

#define T33ms 2666666

#define SPRITE_WIDTH 23  // Width of your sprite (used in FillRect)
#define SPRITE_HEIGHT 26 // Height of your sprite (used in FillRect)

#define PROJECTILE_WIDTH 7
#define PROJECTILE_HEIGHT 15

#define TRANSPARENT_COLOR 0x0000 // Change depending on what transparent color is

#define PROJECTILE_TRANSPARENT_COLOR 0x0000 // Assuming black is transparent

#define NUM_FRAMES 2

#define BACKGROUND_COLOR ST7735_ORANGE 

// Frame arrays for each character
const unsigned short* holtFrames[NUM_FRAMES]    = { holtsprite1, holtsprite2 };
const unsigned short* speightFrames[NUM_FRAMES] = { speightsprite1, speightsprite2 };
const unsigned short* yerrFrames[NUM_FRAMES]    = { yerrsprite1, yerrsprite2 };
const unsigned short* valvFrames[NUM_FRAMES]    = { valvsprite1, valvsprite2 };

uint8_t currentFrame = 0;

// Character draw functions
void DrawHolt(int32_t x, int32_t y) { detectPixels(holtFrames[currentFrame],    x, y - SPRITE_HEIGHT + 1, SPRITE_WIDTH, SPRITE_HEIGHT, TRANSPARENT_COLOR); }

void DrawSpeight (int32_t x, int32_t y) { detectPixels(speightFrames[currentFrame], x, y - SPRITE_HEIGHT + 1, SPRITE_WIDTH, SPRITE_HEIGHT, TRANSPARENT_COLOR); }

void DrawYerr    (int32_t x, int32_t y) { detectPixels(yerrFrames[currentFrame],    x, y - SPRITE_HEIGHT + 1, SPRITE_WIDTH, SPRITE_HEIGHT, TRANSPARENT_COLOR); }

void DrawValv(int32_t x, int32_t y) {
  detectPixels(valvFrames[currentFrame],
               x, y - SPRITE_HEIGHT + 1,
               SPRITE_WIDTH, SPRITE_HEIGHT,
               TRANSPARENT_COLOR);
}

void DrawTitle(uint16_t lang) {
    if(lang == 0)
    SDDraw("title.txt");
    else
    SDDraw("titleS.txt");
}

void DrawCharSel(uint16_t selected) {
    if(selected == 0) {

        ST7735_DrawBitmap(2, 88, yerr, 47, 47);
        ST7735_DrawBitmap(111, 88, holt, 47, 47);
        ST7735_DrawBitmap(56, 48, speight, 47, 47);
        ST7735_DrawBitmap(56, 126, valv, 47, 47);


    // } if (selected == 1) {

    //     ST7735_DrawBitmap(56, 88, holt, 47, 47);
    //     ST7735_SetCursor(10, 9);
    //     ST7735_OutString("Big Sur");
    // } if (selected == 2) {

    //     ST7735_DrawBitmap(56, 88, valv, 47, 47);
    //     ST7735_SetCursor(8, 9);
    //     ST7735_OutString("Dr. Valvano");
    // } if (selected == 4) {

    //     ST7735_DrawBitmap(56, 88, yerr, 47, 47);
    //     ST7735_SetCursor(10, 9);
    //     ST7735_OutString("Dr. Y");
    // }if (selected == 8) {

    //     ST7735_DrawBitmap(56, 88, speight, 47, 47);
    //     ST7735_SetCursor(8, 9);
    //     ST7735_OutString("Dr. Speight");
    }
}

void detectPixels(const uint16_t *sprite, uint16_t x, uint16_t y, uint32_t w, uint32_t h, uint32_t pix){
  for(uint32_t i=0; i<h; i++){
    for(uint32_t j=0; j<w; j++){
      if(sprite[(h-1-i)*w + j] != pix){ // flip Y
        ST7735_DrawPixel(x + j, y + i, sprite[(h-1-i)*w + j]);
      }
    }
  }
}


void eraseOnBackground(const uint16_t *back, uint16_t x, uint16_t y, uint32_t w, uint32_t h){
  for(uint32_t r = 0; r < h; r++){
    for(uint32_t c = 0; c < w; c++){
      ST7735_DrawPixel(x + c, y + r, back[w*r + c]);
    }
  }
}

unsigned short background[23*26];

void eraseOnSprite(int32_t x, int32_t y, int32_t w, int32_t h) {
    for (int i = 0; i < h; i++) {
        for (int j = 0; j < w; j++) {
            ST7735_DrawPixel(x + j, y + i, background[w * i + j]);
        }
    }
}

void Fill(int32_t x, int32_t y, uint16_t w, uint16_t h) {
  ST7735_FillRect(x, y, w, h, BACKGROUND_COLOR);
}

unsigned short projectileBackground[PROJECTILE_WIDTH * PROJECTILE_HEIGHT]; //

void eraseOnSprite(int32_t x, int32_t y, uint16_t w, uint16_t h) {
  Fill(x, y, w, h);
}

void DrawProjectileWithTransparency(int32_t x, int32_t y, const uint16_t* image, uint16_t w, uint16_t h) {
  for (int j = 0; j < h; j++) {
    for (int i = 0; i < w; i++) {
      uint16_t color = image[j * w + i];
      if (color == ST7735_BLACK) {
        ST7735_DrawPixel(x + i, y + j, BACKGROUND_COLOR);
      } else {
        ST7735_DrawPixel(x + i, y + j, color);
      }
    }
  }
}
void FillProjectile(int32_t x, int32_t y, int32_t w, int32_t h) {
  Fill(x, y, w,h);
}

void eraseOnProjectile(int32_t x, int32_t y,int32_t w,int32_t h) {
  FillProjectile(x, y,w,h);
}

void DrawSpriteWithTransparency(int x, int y, const uint16_t* image, int w, int h) {
  for(int row = 0; row < h; row++){
    for(int col = 0; col < w; col++){
      // pull from bottom-up in the source image:
      uint16_t px = image[(h - 1 - row) * w + col];
      if(px == ST7735_BLACK){
        ST7735_DrawPixel(x + col, y + row, BACKGROUND_COLOR);
      } else {
        ST7735_DrawPixel(x + col, y + row, px);
      }
    }
  }
}

void DrawGO(uint16_t sel) {
  if (sel == 1) {
    SDDraw("holtvic.txt");
  } if (sel == 2) {
    SDDraw("valvvic.txt");
  } if (sel == 4) SDDraw("yerrvic.txt");
  if(sel == 8) SDDraw("spvic.txt");
}