// Sound.cpp
// Runs on MSPM0
// Sound assets on SD Card
// Sound assets obtained from https://fankit.supercell.com/d/YvtsWV4pUQVm/audio
// This material is unofficial and is not endorsed by Supercell. For more information see Supercell's Fan Content Policy: www.supercell.com/fan-content-policy.
// Alex Lekhakul & Abhishek Shrestha
#include <cstdint>
#include <stdint.h>
#include <ti/devices/msp/msp.h>
#include "Sound.h"
#include "sounds/sounds.h"
#include "../inc/DAC5.h"
#include "../inc/Timer.h"
#include "ff.h"
#include "diskio.h"
#include "../inc/Clock.h"
#include "../inc/DAC.h"

#define LCD_CS_LOW()    (GPIOB->DOUT31_0 &= ~(1 << 6))   // PB6 = LCD CS
#define LCD_CS_HIGH()   (GPIOB->DOUT31_0 |=  (1 << 6))
#define SD_CS_LOW()     (GPIOA->DOUT31_0 &= ~(1 << 12))  // PA12 = SD CS
#define SD_CS_HIGH()    (GPIOA->DOUT31_0 |=  (1 << 12))

#define BUFSIZE12 1024  // 512 samples * 2 bytes = 1024
uint8_t Buf[BUFSIZE12];
uint8_t Buf2[BUFSIZE12];
uint8_t *front8 = Buf;  // buffer playing
uint8_t *back8 = Buf2;  // buffer loading

volatile int flag8 = 0; // 1 = needs refill
uint32_t Count8 = 0;
FIL SoundFile;
UINT SoundBytesRead;
FRESULT SoundFresult;
uint8_t SoundPlaybackDone = 1; // 1 = no active sound
uint32_t soundLen;
const unsigned char *soundName;
uint32_t soundIndex;
uint32_t bgMusicPosition = 0;


void SysTick_IntArm(uint32_t period, uint32_t priority){
  SysTick->CTRL  = 0x0;      // disable SysTick during setup
  // SysTick->LOAD = 1000000000*period;
  SysTick->LOAD = period - 1;
  SysTick->VAL = 0;
  SCB->SHP[1] = (SCB->SHP[1]&(~0xC0000000)) | (priority << 30);  // set priority = 0 (bits 31,30)
  SysTick->CTRL = 0x07; // enable with core clock and interrupts
}
// initialize a 11kHz SysTick, however no sound should be started
// initialize any global variables
// Initialize the 5 bit DAC
void Sound_Init(void){
// write this
  SysTick_IntArm(80000000/11000,0);
  soundLen = 0;
  soundName = 0;
  soundIndex = 0;
  DAC_Init();

}

// Adapted for SD card reading w buffer
extern "C" void SysTick_Handler(void);
void SysTick_Handler(void){ // called at 11 kHz
  if (Count8 >= BUFSIZE12) {
    uint8_t* temp = front8;
    front8 = back8;
    back8 = temp;
    Count8 = 0;
    flag8 = 1;
  }

  if (SoundPlaybackDone) {
    SysTick->CTRL = 0;
    return;
  }

  // Read 2 bytes and convert to 12-bit
  uint16_t sample12 = (front8[Count8+1] << 8) | front8[Count8];
  DAC_Out(sample12 & 0x0FFF);
  Count8 += 2;
}




void Sound_Stream(const char* filename) {
  f_open(&SoundFile, filename, FA_READ);
  f_read(&SoundFile, Buf, BUFSIZE12, &SoundBytesRead);
  if (SoundBytesRead == 0) {
    f_close(&SoundFile);
    return;
  }

  f_read(&SoundFile, Buf2, BUFSIZE12, &SoundBytesRead);

  front8 = Buf;
  back8 = Buf2;
  Count8 = 0;
  flag8 = 0;
  SoundPlaybackDone = 0;

  SysTick->VAL = 0;
  SysTick->CTRL = 0x07;  // enable SysTick
}

void Sound(const char* filename) {
  Sound_Stream(filename);

  while (!SoundPlaybackDone) {
    if (flag8) {
      flag8 = 0;
      FRESULT res = f_read(&SoundFile, back8, BUFSIZE12, &SoundBytesRead);
      if (res != FR_OK || SoundBytesRead < BUFSIZE12) {
        f_close(&SoundFile);
        SoundPlaybackDone = 1;
        SysTick->CTRL = 0;
      }
    }
  }
}


void soundBg(const char* filename) {
  SoundFresult = f_open(&SoundFile, filename, FA_READ);
  if (SoundFresult) return;

  f_read(&SoundFile, Buf, BUFSIZE12, &SoundBytesRead);
  if (SoundBytesRead == 0) {
    f_close(&SoundFile);
    return;
  }

  f_read(&SoundFile, Buf2, BUFSIZE12, &SoundBytesRead);

  front8 = Buf;
  back8 = Buf2;
  Count8 = 0;
  flag8 = 0;
  SoundPlaybackDone = 0;

  SysTick->VAL = 0;
  SysTick->CTRL = 0x07;
}


void loopBg(void) {
  if (flag8 && !SoundPlaybackDone) {
    flag8 = 0;

    // Check if we're about to run out of file
    FRESULT res = f_read(&SoundFile, back8, BUFSIZE12, &SoundBytesRead);
    
    if (res != FR_OK || SoundBytesRead < BUFSIZE12) {

      f_lseek(&SoundFile, 0);
      f_read(&SoundFile, back8, BUFSIZE12, &SoundBytesRead);
    }
  }
}



void swapBg(const char* filename) {
  // Stop and close the current file
  SoundPlaybackDone = 1;
  SysTick->CTRL = 0;
  f_close(&SoundFile);
  bgMusicPosition = 0;
  // Start new track
  soundBg(filename);
}


void pauseBg(void) {
  bgMusicPosition = f_tell(&SoundFile);  // Save current byte offset
  SysTick->CTRL = 0;                     // Stop playback timer
  SoundPlaybackDone = 1;                // Mark playback as stopped
}

void resumeBg(const char* filename) {
  FRESULT res = f_open(&SoundFile, filename, FA_READ);
  if (res != FR_OK) return;

  // find resume place
  f_lseek(&SoundFile, bgMusicPosition);

  // prime both buffers
  f_read(&SoundFile, Buf, BUFSIZE12, &SoundBytesRead);
  f_read(&SoundFile, Buf2, BUFSIZE12, &SoundBytesRead);

  front8 = Buf;
  back8 = Buf2;
  Count8 = 0;
  flag8 = 0;
  SoundPlaybackDone = 0;

  SysTick->VAL = 0;
  SysTick->CTRL = 0x07;  // resume playback
}



//******* Sound_Start ************
// This function does not output to the DAC. 
// Rather, it sets a pointer and counter, and then enables the SysTick interrupt.
// It starts the sound, and the SysTick ISR does the output
// feel free to change the parameters
// Sound should play once and stop
// Input: pt is a pointer to an array of DAC outputs
//        count is the length of the array
// Output: none
// special cases: as you wish to implement

void Sound_Start(const uint8_t *pt, uint32_t count){
  soundName = pt;
  soundLen = count;
  soundIndex = 0;
  SysTick->VAL = 0;
  SysTick->CTRL = 0x07;
  
}

void Sound_Shoot(void){
// write this
  Sound_Start(shoot, 4080);
}
void Sound_vb(void){
Sound_Start(vineboom, 13836);
}

void Sound_amelia(void) {
Sound_Start(ameliatest, 22784);
}
