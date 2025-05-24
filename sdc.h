// SDCard.h
// Header for SD card mount utility

#ifndef SDCARD_H
#define SDCARD_H

#include "ff.h"  // FatFs declarations

extern FATFS g_sFatFs;  // Global file system object

// Call this once during startup
void SD_Init(void);

#endif // SDCARD_H