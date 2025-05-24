// SDCard.c
// SD card mount with LCD-safe chip select handling
#include "sdc.h"
#include "ff.h"
#include <ti/devices/msp/msp.h>
#include "../inc/ST7735.h"

FATFS g_sFatFs;

// Chip select pins
#define LCD_CS_PIN   6   // PB6
#define SD_CS_PIN    12  // PA12

// CS control macros
#define LCD_CS_LOW()    (GPIOB->DOUT31_0 &= ~(1 << LCD_CS_PIN))
#define LCD_CS_HIGH()   (GPIOB->DOUT31_0 |=  (1 << LCD_CS_PIN))
#define SD_CS_LOW()     (GPIOA->DOUT31_0 &= ~(1 << SD_CS_PIN))
#define SD_CS_HIGH()    (GPIOA->DOUT31_0 |=  (1 << SD_CS_PIN))

void SD_Init(void) {

  LCD_CS_HIGH();
  SD_CS_LOW();

  FRESULT res = f_mount(&g_sFatFs, "", 0);

  SD_CS_HIGH();
  LCD_CS_LOW();

  ST7735_InitPrintf();
  ST7735_SetRotation(1);
  ST7735_FillScreen(ST7735_BLACK);

  if (res != FR_OK) {
    ST7735_SetCursor(0, 0);
    ST7735_OutString("SD mount failed");
    while (1); // stop if failed
  }
}