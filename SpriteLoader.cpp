// Alex Lekhakul
// SD Card image function
// 4/20/2025

#include "ff.h"
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "ff.h"
#include "ffconf.h"
#include "SpriteLoader.h"
#include "ST7735_SDC.h"


uint16_t pixelBuffer[128];  // Only 256 bytes of RAM

void SDDraw(const char *file) {
    FIL imgFile;
    char buffer[128];
    UINT count = 0;
    uint32_t totalPixels = 0;

    if (f_open(&imgFile, file, FA_READ) != FR_OK) return;

    // Configure screen orientation and drawing region
    TFT_OutCommand(0x36);      // MADCTL
    TFT_OutData(0x48);         // row/column exchange

    TFT_OutCommand(0x2A);      // Column address set
    TFT_OutData(0x00); TFT_OutData(0);
    TFT_OutData(0x00); TFT_OutData(127);

    TFT_OutCommand(0x2B);      // Row address set
    TFT_OutData(0x00); TFT_OutData(0);
    TFT_OutData(0x00); TFT_OutData(159);

    TFT_OutCommand(0x2C);      // RAM write

    // Advance to the first opening brace
    while (f_gets(buffer, sizeof(buffer), &imgFile)) {
        if (strchr(buffer, '{')) break;
    }

    // Read and stream pixel data to display
    while (f_gets(buffer, sizeof(buffer), &imgFile) && totalPixels < 128 * 160) {
        char *val = strtok(buffer, ",{} \t\n\r");
        while (val && count < 128) {
            pixelBuffer[count++] = (uint16_t)strtol(val, NULL, 0);
            val = strtok(NULL, ",{} \t\n\r");
        }

        for (uint16_t i = 0; i < count; i++) {
            uint16_t color = pixelBuffer[i];
            TFT_OutData(color >> 8);     // High byte
            TFT_OutData(color & 0xFF);   // Low byte
            totalPixels++;
        }
        count = 0;
    }

    f_close(&imgFile);
    ST7735_SetRotation(1);  // Reset orientation if needed
}