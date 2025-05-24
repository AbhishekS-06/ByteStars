// Lab9HMain.cpp
// Byte Stars
// Runs on MSPM0G3507
// Lab 9 ECE319H
// Alex Lekhakul & Abhishek Shrestha
// Last Modified: 4/24/2025

#include <cstdint>
#include <cstdio>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <ti/devices/msp/msp.h>
#include "ST7735_SDC.h"
#include "../inc/Clock.h"
#include "../inc/LaunchPad.h"
#include "../inc/TExaS.h"
#include "../inc/Timer.h"
#include "../inc/SlidePot.h"
#include "../inc/DAC5.h"
#include "SmallFont.h"
#include "LED.h"
#include "Switch.h"
#include "Sound.h"
#include "images/byteStars/images.h"
#include "Background.h"
#include "ff.h"
#include "diskio.h"
#include "sdc.h"
#include "SpriteLoader.h"
#include "Joystick.h"
#include "../inc/ADC.h"
#include "sprite.h"
#include "ti/devices/msp/m0p/mspm0g350x.h"
#include "ti/devices/msp/peripherals/hw_adc12.h"

extern "C" void __disable_irq(void);
extern "C" void __enable_irq(void);
extern "C" void TIMG12_IRQHandler(void);

#define REDL (1<<16)
#define GREEN1 (1<<17)
#define GREEN2 (1<<19)
#define YELLOWL (1<<24)
#define BUFSIZE12 1024

#define SCREEN_WIDTH 160 // Width of ST7735 screen after rotation
#define SCREEN_HEIGHT 128 // Height of ST7735 screen after rotation

#define SPRITE_WIDTH 23  // Width of your sprite (used in FillRect)
#define SPRITE_HEIGHT 26 // Height of your sprite (used in FillRect)
#define YINT_WIDTH  15
#define YINT_HEIGHT 19
#define YINT_DAMAGE 2      // less than other attacks
#define FREEZE_TICKS (30*2) // 2 seconds @ 30Hz
const uint16_t* yintFrames[2] = { particle1,particle2};
#define PROJECTILE_WIDTH 7
#define PROJECTILE_HEIGHT 15

#define X_STEP_SIZE 3 // How many pixels to move horizontally per frame
#define Y_STEP_SIZE 2 // How many pixels to move vertically per frame

#define JOYSTICK_CENTER_X 2260
#define JOYSTICK_CENTER_Y 1548
#define DEADZONE_THRESHOLD 200

#define BACKGROUND_COLOR ST7735_ORANGE 

#define NUM_FRAMES 2

#define G1COOLDOWN 4*30
#define G2COOLDOWN 4*30
// DAMAGE & HEALTH
#define ENEMY_MAX_HEALTH 5
#define DMG_MUST         2
#define DMG_DEREF        1
#define DMG_POINT        3

extern const unsigned short* holtFrames[NUM_FRAMES];

extern const unsigned short* speightFrames[NUM_FRAMES];

extern int flag8;                 // set by SysTick to trigger refill
extern int SoundPlaybackDone;     // 1 when done playing
extern FIL SoundFile;             // open sound file
extern UINT SoundBytesRead;       // how much was read
extern uint8_t *back8;
FATFS fs;
// ****note to ECE319K students****
// the data sheet says the ADC does not work when clock is 80 MHz
// however, the ADC seems to work on my boards at 80 MHz
// I suggest you try 80MHz, but if it doesn't work, switch to 40MHz
void PLL_Init(void){ // set phase lock loop (PLL)
  // Clock_Init40MHz(); // run this line for 40MHz
  Clock_Init80MHz(0);   // run this line for 80MHz
}

uint32_t Time;
uint32_t M=1;
uint32_t Random32(void){
  M = 1664525*M+1013904223;
  return M;
}
uint32_t Random(uint32_t n){
  return (Random32()>>16)%n;
}

SlidePot Sensor(1921,121);
uint32_t Data;
uint32_t now;
uint32_t slideNow;
uint32_t semaphore;
uint32_t ammoSem;
uint32_t rawX;
uint32_t rawY;
uint32_t g1act = 0;
uint32_t g2act = 0;




#define T33ms 2666666

// games  engine runs at 30Hz
void TIMG12_IRQHandler(void){uint32_t pos,msg;
  if((TIMG12->CPU_INT.IIDX) == 1){ // this will acknowledge
    GPIOB->DOUTTGL31_0 = GREEN; // toggle PB27 (minimally intrusive debugging)
    GPIOB->DOUTTGL31_0 = GREEN; // toggle PB27 (minimally intrusive debugging)
// game engine goes here
    Time++;
    // 1) sample slide pot
    Data = Sensor.In();
    slideNow = Sensor.Convert(Data);
    JoyStick_In(&rawY, &rawX);
    rawX = 4095 - rawX;
    // 2) read input switches
    now = Switch_In();
    // 3) move sprites
    static uint8_t animCounter = 0;
    animCounter++;
    if (animCounter >= 3) { // 30Hz / 3 = 10Hz animation
      animCounter = 0;
      currentFrame = (currentFrame + 1) % NUM_FRAMES;
    }
    // 4) start sounds
    // 5) set semaphore
    semaphore = 1;
    // NO LCD OUTPUT IN INTERRUPT SERVICE ROUTINES
    GPIOB->DOUTTGL31_0 = GREEN; // toggle PB27 (minimally intrusive debugging)
  }
}
uint8_t TExaS_LaunchPadLogicPB27PB26(void){
  return (0x80|((GPIOB->DOUT31_0>>26)&0x03));
}

typedef enum {English, Spanish, Portuguese, French} Language_t;
Language_t myLanguage=English;
typedef enum {HELLO, GOODBYE, LANGUAGE} phrase_t;
const char Hello_English[] ="Hello";
const char Hello_Spanish[] ="\xADHola!";
const char Hello_Portuguese[] = "Ol\xA0";
const char Hello_French[] ="All\x83";
const char Goodbye_English[]="Goodbye";
const char Goodbye_Spanish[]="Adi\xA2s";
const char Goodbye_Portuguese[] = "Tchau";
const char Goodbye_French[] = "Au revoir";
const char Language_English[]="English";
const char Language_Spanish[]="Espa\xA4ol";
const char Language_Portuguese[]="Portugu\x88s";
const char Language_French[]="Fran\x87" "ais";
const char *Phrases[3][4]={
  {Hello_English,Hello_Spanish,Hello_Portuguese,Hello_French},
  {Goodbye_English,Goodbye_Spanish,Goodbye_Portuguese,Goodbye_French},
  {Language_English,Language_Spanish,Language_Portuguese,Language_French}
};

int32_t XData, YData;
int16_t oldX, oldY;

int mainSD(void) {
  PLL_Init();
  LaunchPad_Init();

  disk_initialize(0);

  // Mount the filesystem
  FRESULT mountResult = f_mount(&fs, "", 0);
  if (mountResult != FR_OK) {
    return 1;  // Mount failed
  }

  ST7735_InitPrintf();
  ST7735_DrawString(0, 0, (char*)"Loading image...", ST7735_YELLOW);

  // Load and display image
  SDDraw("chars.txt");
  printf("sugoi");
  SDDraw("titleS.txt");
  while(1);
}

#define CLAMP(v, lo, hi)  ((v)<(lo)?(lo):((v)>(hi)?(hi):(v)))

extern const unsigned short* holtFrames[NUM_FRAMES];
extern const unsigned short* speightFrames[NUM_FRAMES];
extern const unsigned short* yerrFrames[NUM_FRAMES];
extern const unsigned short* valvFrames[NUM_FRAMES];

#define HOLT_SHOT_W 13
#define HOLT_SHOT_H 10

#define MUSTACHE_FRAMES 4
const uint16_t* mustFrames[MUSTACHE_FRAMES] = { must1, must2, must3, must4 };
uint8_t mustFrameIndex = 0;   // global or static in main

int main(void) {
  int16_t oldX, oldY;

  int16_t oldProjX, oldProjY;

  uint8_t projectileWasActive = 0;

  uint8_t projectileFrameIndex = 0; // Animation frame tracker

  // mustache animation index for Holt’s shot
  uint8_t mustFrameIndex = 0;

  __disable_irq();
  PLL_Init();
  LaunchPad_Init();
  __enable_irq();
  disk_initialize(0);
  __disable_irq();
  Clock_Init40MHz();
  ST7735_InitR(INITR_REDTAB);
  if (f_mount(&fs, "", 0) != FR_OK) return 1;
  ST7735_InitPrintf();
  ST7735_FillScreen(ST7735_BLACK);
  Sensor.Init();
  Switch_Init();
  LED_Init();
  Sound_Init();
  JoyStick_Init();
  TExaS_Init(0, 0, &TExaS_LaunchPadLogicPB27PB26);
  TimerG12_IntArm(80000000/30, 1);
  Time = 0;
  __enable_irq();

  // projectile sprites
  const uint16_t* derefFrames[] = { deref1,    deref2    };
  Sprite deref = {0,0,0,0,derefFrames[0],7,15,0};
  const uint16_t* pointFrames[] = { point1,    point2    };
  Sprite point = {0,0,0,0,pointFrames[0],6,23,0};
  const uint8_t PROJECTILE_NUM_FRAMES = 2;

  // right after you declare your other Sprite instances, before the game‐loop:
  Sprite mustShot = {
    .x     = 0,
    .y     = 0,
    .vx    = 0,
    .vy    = 0,
    .image = mustFrames[0],  // use our new array
    .w     = 13,
    .h     = 10,
    .life  = 0
  };
Sprite msp = {0,0,0,0,mspm0,10,14,0};
  Sprite yint = {0,0,0,0,yintFrames[0], 15, 19, 0};
  uint8_t yintFrameIndex = 0;
  static uint32_t freezeEndTime = 0;

  uint32_t borderStart = 0;
  bool     borderOn    = false;
  const uint32_t BORDER_TICKS = 4 * 30;  // 4 seconds @ 30Hz

  // in main(), before your while(1)…
  bool    freezeActive    = false;    // are we currently frozen?
  uint32_t freezeStartTime = 0;       // Time tick when freeze began
  const uint32_t YERR_FREEZE_TICKS = 2 * 30; // 2 seconds @ 30Hz

  uint32_t playerHealth        = 3;    // player starts with 3 health
  const uint32_t DMG_FROM_ENEMY = 1;   // each collision costs 1 health

  uint32_t score = 0;

  // track kills (increment whenever you handle a death here)
  static uint32_t killCount = 0;

  // Increases by 1 every time you hit a multiple of 3 kills
  static uint32_t enemySpeedLevel = 0;

  // --- Holt’s mustache attack animation frames ---
  #define MUSTACHE_FRAMES 4
  const uint16_t* mustFrames[MUSTACHE_FRAMES] = {
    must1,
    must2,
    must3,
    must4
  };

// Three Enemy2 AI bots
Sprite enemyBot[3] = {
{ .x = SCREEN_WIDTH/6,   .y = SPRITE_HEIGHT, .vx = 1, .vy = 0, .image = Enemy2, .w = 14, .h = 10, .life = 1 },  // always active
{ .x = SCREEN_WIDTH/2,   .y = SPRITE_HEIGHT, .vx = 1, .vy = 0, .image = Enemy2, .w = 14, .h = 10, .life = 0 },  // unlock at  5 kills
{ .x = SCREEN_WIDTH*5/6, .y = SPRITE_HEIGHT, .vx = 1, .vy = 0, .image = Enemy2, .w = 14, .h = 10, .life = 0 }   // unlock at 10 kills
};
uint32_t enemyHealth[3] = { ENEMY_MAX_HEALTH, ENEMY_MAX_HEALTH, ENEMY_MAX_HEALTH };

////////////////////////////////////////////////////////////////////////////////////
  uint16_t titleFlag = 1, drawn = 0, music = 0;
  uint16_t selLine = 0, langFlag = 0, charFlag = 0;
  uint16_t selFlag =0, selscFlag = 0, gameFlag = 0, joyFlag = 0;
  uint16_t poinFlag = 0, redLedSem = 0, firstFire = 0, valvFlag = 0;
  uint32_t ammoSet = 0, slideLast = 0, swNow = 0, swLast = 0, g1flag = 1;uint32_t g2flag = 1; uint16_t soundFlag = 0;
  Sound("intro.bin");
  soundBg("introbg.bin");
  pauseBg();
  while (1) {
    Sensor.Sync();
    // wait for semaphore
       // clear semaphore
       // update ST7735R
    GPIOB->DOUTTGL31_0 = RED; // toggle PB26 (minimally intrusive debugging)
    GPIOB->DOUTTGL31_0 = RED; // toggle PB26 (minimally intrusive debugging)
    // Title
  
    if(titleFlag == 1) {
      if (drawn == 0) DrawTitle(langFlag);
      drawn = 1;
      if (music == 0) resumeBg("introbg.bin");
      music = 1;
      // Language Select
      if (now != swLast)
        if (now == 1) {
          langFlag = !langFlag;
          drawn = 0;
          music = 0;
          pauseBg();
          Sound("sel.bin");
        }
        // Game
        if (now == 15) {
          titleFlag = 0;
          music = 0;
          pauseBg();
          ST7735_FillScreen(ST7735_BLACK);
          gameFlag = 1;
          swapBg("battleBg.bin");
        }
        // Char Sel
        if (now == 4) {
          titleFlag = 0;
          music = 0;
          drawn = 0;
          pauseBg();
          Sound("sel.bin");
          charFlag = 1;
          swapBg("selBg");
        }
      swLast = now;
      Clock_Delay1ms(10);
    }
    
    // Char sel
    if (charFlag == 1) {
      if (drawn == 0) {
      if(langFlag == 0) SDDraw("chars.txt");
          else SDDraw("charsS.txt");
          DrawCharSel(0);
      }
      drawn = 1;
      if (music == 0) resumeBg("selBg.bin");
      music = 1;
      if (now != swLast){
          if (now==1||now==2||now==4||now==8){
            //  if (slideNow == 0) DrawCharSel(now);
            //  if (slideNow == 1) {
                charFlag = 0;
                selFlag = now;
                selscFlag = 1;
                Sound("sel.bin");
                drawn = 0;
                music = 0;
                selLine  = 0;
                swapBg("battleBg.bin");
              }
            // } 
        }
    swLast = now;
    Clock_Delay1ms(10);
    }

    // Speight
    if (selFlag == 8 && selscFlag == 1) {
      if (drawn == 0) SDDraw("spsel.txt");
      drawn = 1;
      if (selLine == 0) Sound("spSel.bin");
      selLine = 1;
      if (music == 0) {
        resumeBg("battleBg.bin");
        music = 1;
      }
      if (now != swLast) {
        if (now == 4) {
          joyFlag = 1;
          selscFlag = 0;
          pauseBg();
        }
        if (now == 1) {
          selFlag = 0;
          selscFlag = 0;
          charFlag = 1;
          drawn = 0;
          music = 0;
          pauseBg();
        }
      }
      swLast = now;
    }
  
    // Yerr
    if (selFlag == 4 && selscFlag == 1) {
      if (drawn == 0) SDDraw("yerrsel.txt");
      drawn = 1;
      if(selLine == 0) Sound("ysel.bin");
      selLine = 1;
        if (music == 0) {
        resumeBg("battleBg.bin");
        music = 1;
      }
      if (now != swLast) {
        if (now == 4) {
          joyFlag = 1;
          selscFlag = 0;
          pauseBg();
        }
        if (now == 1) {
          selFlag = 0;
          charFlag = 1;
          selscFlag = 0;
          drawn = 0;
          music = 0;
          pauseBg();
        }
      }
      swLast = now;
    }

    // Valv
    if (selFlag == 2 && selscFlag == 1) {
      if (drawn == 0) SDDraw("valvsel.txt");
      drawn = 1;
      if (selLine == 0) Sound("valvSel.bin");
      selLine = 1;
      if (music == 0) {
        resumeBg("battleBg.bin");
        music = 1;
      }
      if (now != swLast) {
        if (now == 4) {
          joyFlag = 1;
          selscFlag = 0;
          pauseBg();
        }
        if (now == 1) {
          selFlag = 0;
          charFlag = 1;
          selscFlag = 0;
          drawn = 0;
          music = 0;
          pauseBg();
        }
      }
      swLast = now;
    }

    // Holt
    if (selFlag == 1 && selscFlag == 1) {
      if (drawn == 0 && langFlag == 0) SDDraw("holtsel.txt");
      if (drawn == 0 && langFlag == 1) SDDraw("holtselS.txt");
      drawn = 1;
      if (selLine == 0) Sound("hSel.bin");
      selLine = 1;
      if (music == 0) {
        resumeBg("battleBg.bin");
        music = 1;
      }
      if (now != swLast) {
        if (now == 4) {
          joyFlag = 1;
          selscFlag = 0;
          pauseBg();
        }
        if (now == 1) {
          selFlag = 0;
          charFlag = 1;
          selscFlag = 0;
          drawn = 0;
          music = 0;
          pauseBg();
        }
      }
      swLast = now;
    }


    // if (gameFlag == 1) {
    //   ST7735_SetRotation(1);
    //   ST7735_SetCursor(10,5);
    //   // Primary Fire Ammo Slidepot stuff
      
    //   if (slideNow != slideLast) {
    //     if (ammoSem == 0 && redLedSem == 0) {
    //       if (ammoSet < 5) ammoSet += slideNow;
    //     } 
    //   }
    //   slideLast = slideNow;
    //   if (ammoSet == 0) {
    //     LED_On(REDL, 0);
    //     redLedSem = 0;
    //   } else if (redLedSem == 0 && ammoSet == 5) {
    //     LED_Off(REDL, 0);
    //     redLedSem = 1;
    //   }
    //   swNow = now;
    //   if (swNow != swLast) {
    //     if (now == 4 && ammoSet > 0 && redLedSem == 1) ammoSet -= 1;
        
    //     if (now == 15) {
    //       titleFlag = 1;
    //       gameFlag = 0;
    //       drawn = 0;
    //       music = 0;
    //       pauseBg();
    //       LED_Off(REDL, 0);
    //     }
    //   }
    //   swLast = swNow;
    //   Clock_Delay1ms(10);
    // // move cursor to top
    //   ST7735_SetCursor(0, 0);
    // // display distance in top row OutFix
    //   printf("Charge: %d\n", ammoSet);
    //   printf("Switch Input: %d\n", now);
    //   // check for end game or level switch
    // }

    // when ready to run the sprite demo:
    if (joyFlag == 1) {

      // pick correct draw routine & frames
      void (*DrawCurrent)(int32_t,int32_t);
      const unsigned short** CurrentFrames;
      switch (selFlag) {
        case 1: DrawCurrent = DrawHolt;    CurrentFrames = holtFrames;    break;
        case 8: DrawCurrent = DrawSpeight; CurrentFrames = speightFrames; break;
        case 4: DrawCurrent = DrawYerr;    CurrentFrames = yerrFrames;    break;
        case 2: DrawCurrent = DrawValv;    CurrentFrames = valvFrames;    break;
        default:DrawCurrent = DrawSpeight; CurrentFrames = speightFrames; break;
      }

      // draw initial enemies
      for(int i = 0; i < 3; i++){
        if(enemyBot[i].life){
          DrawSpriteWithTransparency(
            enemyBot[i].x, enemyBot[i].y,
            enemyBot[i].image,
            enemyBot[i].w, enemyBot[i].h
          );
        }
      }


      ST7735_SetRotation(1);
      ST7735_FillScreen(BACKGROUND_COLOR);

      XData = (SCREEN_WIDTH - SPRITE_WIDTH)/2;
      YData = (SCREEN_HEIGHT + SPRITE_HEIGHT)/2;
      Fill(XData, YData, SPRITE_WIDTH, SPRITE_HEIGHT);
      DrawCurrent(XData, YData);
      // erase previous HP text area (assumes your font is 6×8px)
      ST7735_FillRect(0, 0, 6*3, 8, BACKGROUND_COLOR); 

      // draw the updated HP
      ST7735_SetCursor(0, 0);
      char buf[8];
      snprintf(buf, sizeof(buf), "HP:%u", playerHealth);
      ST7735_OutString(buf);

      static uint32_t prevNow = 0;

      while (1) {
                          if (g1flag == 0) {
  if ((Time - g1act) >= G1COOLDOWN) {
    g1flag = 1;
  }
}
if (g2flag == 0) {
  if ((Time - g2act) >= G2COOLDOWN) {
    g2flag = 1;
  }
}
// G1 Ready Indicator
if ((Time - g1act) >= G1COOLDOWN) {
  LED_On(GREEN1, 0); // G1 ready
} else {
  LED_Off(GREEN1, 0); // G1 cooling down
}

// G2 Ready Indicator
if ((Time - g2act) >= G2COOLDOWN) {
  LED_On(0, GREEN2); // G2 ready
} else {
  LED_Off(0,GREEN2); // G2 cooling down
}
        oldX = XData; oldY = YData;

        // 1) RISING-EDGE & FREEZE TIMER
        if (selFlag == 8 && now == 8 && prevNow != 8 && !freezeActive && g1flag ==1) {
          Sound("spSegF.bin");
          g1act = Time;
          g1flag = 0;
          freezeActive    = true;
          freezeStartTime = Time;
        }
        prevNow = now;
        if (freezeActive && (Time - freezeStartTime) >= FREEZE_TICKS) {
          freezeActive = false;
        }

        // rising edge of Yerr button
        if (selFlag == 4 && now == 8 && prevNow != 8) {
          // start shield
          borderOn       = true;
          borderStart    = Time;
        }

        // 2) PLAYER MOVEMENT (always runs)
        // … your existing joystick + redraw code …

        // 3) ENEMY CHASE (only when not frozen)
        if (!freezeActive) {
          for (int i = 0; i < 3; ++i) {
            if (!enemyBot[i].life) continue;
            int16_t exO = enemyBot[i].x, eyO = enemyBot[i].y;
            int speed = 1 + enemySpeedLevel;
            // chase logic…
            if (XData < exO)                    enemyBot[i].x -= speed;
            else if (XData > exO + enemyBot[i].w) enemyBot[i].x += speed;
            int16_t py = YData - SPRITE_HEIGHT + 1;
            if (py < eyO)                       enemyBot[i].y -= speed;
            else if (py > eyO + enemyBot[i].h)    enemyBot[i].y += speed;

            // redraw enemy[i]
            ST7735_FillRect(exO, eyO, enemyBot[i].w, enemyBot[i].h, BACKGROUND_COLOR);
            DrawSpriteWithTransparency(
              enemyBot[i].x, enemyBot[i].y,
              enemyBot[i].image,
              enemyBot[i].w, enemyBot[i].h
            );
          }
        }

        int16_t x_step=0, y_step=0;
        if (rawX > JOYSTICK_CENTER_X + DEADZONE_THRESHOLD)      x_step =  X_STEP_SIZE;
        else if (rawX < JOYSTICK_CENTER_X - DEADZONE_THRESHOLD) x_step = -X_STEP_SIZE;
        if (rawY > JOYSTICK_CENTER_Y + DEADZONE_THRESHOLD)      y_step = -Y_STEP_SIZE;
        else if (rawY < JOYSTICK_CENTER_Y - DEADZONE_THRESHOLD) y_step =  Y_STEP_SIZE;

        XData = CLAMP(XData + x_step, 0, SCREEN_WIDTH - SPRITE_WIDTH);
        YData = CLAMP(YData + y_step, SPRITE_HEIGHT, SCREEN_HEIGHT);

        if (XData!=oldX || YData!=oldY) {
          ST7735_FillRect(oldX, oldY - SPRITE_HEIGHT + 1,
                          SPRITE_WIDTH, SPRITE_HEIGHT,
                          BACKGROUND_COLOR);
          DrawSpriteWithTransparency(
            XData, YData - SPRITE_HEIGHT + 1,
            CurrentFrames[currentFrame],
            SPRITE_WIDTH, SPRITE_HEIGHT);
        }

        // each frame, before drawing enemies/projectiles, clear the old HP
        ST7735_FillRect(0, 0, 6*4, 8, BACKGROUND_COLOR); // make room for “HP:3”

        // draw current HP
        ST7735_SetCursor(0, 0);
        char hpBuf[8];
        snprintf(hpBuf, sizeof(hpBuf), "HP:%u", playerHealth);
        ST7735_OutString(hpBuf);

        // --- Enemy2 AI: chase the player (only when not frozen) ---
        if (!freezeActive) {
          for (int i = 0; i < 3; ++i) {
            if (!enemyBot[i].life) continue;
            int16_t oldX = enemyBot[i].x, oldY = enemyBot[i].y;
            int chaseSpeed = 1 + enemySpeedLevel;

            // chase horizontally
            if      (XData < oldX)                      enemyBot[i].x -= chaseSpeed;
            else if (XData > oldX + enemyBot[i].w)      enemyBot[i].x += chaseSpeed;
            // chase vertically (using sprite top)
            int16_t py = YData - SPRITE_HEIGHT + 1;
            if      (py < oldY)                         enemyBot[i].y -= chaseSpeed;
            else if (py > oldY + enemyBot[i].h)         enemyBot[i].y += chaseSpeed;

            // erase old and redraw
            ST7735_FillRect(oldX, oldY,
                            enemyBot[i].w, enemyBot[i].h,
                            BACKGROUND_COLOR);
            DrawSpriteWithTransparency(
              enemyBot[i].x, enemyBot[i].y,
              enemyBot[i].image,
              enemyBot[i].w, enemyBot[i].h
            );
          }
        }

//yerr
        if (selFlag == 4) {
          // start border on new press
          if (now == 8 && !borderOn && g1flag == 1) {
           
              Sound("ybuf.bin");
              g1act = Time;
           g1flag = 0;
            
            borderOn    = true;
            borderStart = Time;
          }
          
          // heal 1 HP
          if (now == 2 && g2flag == 1) {
            Sound("yalign.bin");
            g2act = Time;
            g2flag = 0;
            if (playerHealth < 3) {
              playerHealth++;
              // optional feedback flash
              LED_On(GREEN1, 0);
              Clock_Delay1ms(50);
              LED_Off(GREEN1, 0);
            }
          }
          // if border active, draw for 4s then turn off
          if (borderOn) {
            if ((Time - borderStart) < BORDER_TICKS) {
              int bx = XData;
              int by = YData - SPRITE_HEIGHT + 1;
              int bw = SPRITE_WIDTH;
              int bh = SPRITE_HEIGHT;
              // top & bottom edges
              for (int i = 0; i < bw; ++i) {
                ST7735_DrawPixel(bx + i,      by,        ST7735_BLUE);
                ST7735_DrawPixel(bx + i,      by + bh - 1, ST7735_BLUE);
              }
              // left & right edges
              for (int j = 0; j < bh; ++j) {
                ST7735_DrawPixel(bx,           by + j, ST7735_BLUE);
                ST7735_DrawPixel(bx + bw - 1,  by + j, ST7735_BLUE);
              }
            } else {
              borderOn = false;
            }
          }
          // inside the “if(selFlag==4)” block, after your shield code…
          if((now & 0x04) && !yint.life){
            // fire yint when button pressed, if not already active
            if (firstFire == 0) {
              Sound("yint.bin");
              firstFire = 1;
            }
            yint.life = 1;
            yint.x    = XData + (SPRITE_WIDTH/2) - (YINT_WIDTH/2);
            yint.y    = YData - SPRITE_HEIGHT - YINT_HEIGHT;
            yint.vx   = 0;
            yint.vy   = -6;
            // reset animation
            yintFrameIndex = 0;
            yint.image      = yintFrames[0];
          }
          // update & erase old yint
          if(yint.life){
            oldProjX = yint.x; oldProjY = yint.y;
            yint.x += yint.vx; yint.y += yint.vy;
            if((yint.y + yint.h) < 0) yint.life = 0;
            ST7735_FillRect(oldProjX, oldProjY, yint.w, yint.h, BACKGROUND_COLOR);
          }
          // draw new yint frame
          if(yint.life){
            yintFrameIndex = (yintFrameIndex + 1) % 2;
            yint.image      = yintFrames[yintFrameIndex];
            DrawProjectileWithTransparency(
              yint.x, yint.y,
              yint.image,
              yint.w, yint.h
            );
          }
        }

        // --- Speight’s attacks (swap deref <-> pointer each press) ---
        if(selFlag == 8) {

          if(now == 2) Sound("spTree.bin");
          if(now == 1) Sound("spHP.bin");
          if (now == 4){
          if (firstFire == 0)
          Sound("spDer.bin");
        firstFire = 1;
          // 1) Fire new projectile if both are dead
          if((now & 0x04) && deref.life == 0 && point.life == 0) {
            if(poinFlag == 0) {
              // Fire deref
              deref.life = 1;
              deref.x    = XData + (SPRITE_WIDTH/2) - (deref.w/2);
              deref.y    = YData - SPRITE_HEIGHT - deref.h + 4;
              deref.vx   = 0;
              deref.vy   = -6;
              poinFlag   = 1;               // next shot will be pointer
            } else {
              if (firstFire == 0)
          Sound("spPoin.bin");
        firstFire = 1;
              // Fire pointer
              point.life = 1;
              point.x    = XData + (SPRITE_WIDTH/2) - (point.w/2);
              point.y    = YData - SPRITE_HEIGHT - point.h + 4;
              point.vx   = 0;
              point.vy   = -10;
              poinFlag   = 0;               // next shot will be deref
            }
          }

          // 2) Update & erase old deref
          if(deref.life) {
            oldProjX = deref.x;
            oldProjY = deref.y;
            deref.x += deref.vx;
            deref.y += deref.vy;
            if((deref.y + deref.h) < 0) {
              deref.life = 0;
            }
            // erase exactly the deref rectangle
            ST7735_FillRect(oldProjX, oldProjY,
                            deref.w, deref.h,
                            BACKGROUND_COLOR);
          }
          // draw new deref
          if(deref.life) {
            projectileFrameIndex = (projectileFrameIndex + 1) % PROJECTILE_NUM_FRAMES;
            deref.image = derefFrames[projectileFrameIndex];
            DrawProjectileWithTransparency(
              deref.x, deref.y,
              deref.image,
              deref.w, deref.h
            );
          }

          // 3) Update & erase old pointer
          if(point.life) {
            oldProjX = point.x;
            oldProjY = point.y;
            point.x += point.vx;
            point.y += point.vy;
            if((point.y + point.h) < 0) {
              point.life = 0;
            }
            // erase exactly the pointer rectangle
            ST7735_FillRect(oldProjX, oldProjY,
                            point.w, point.h,
                            BACKGROUND_COLOR);
          }
          // draw new pointer
          if(point.life) {
            DrawProjectileWithTransparency(
              point.x, point.y,
              point.image,
              point.w, point.h
            );
          }
        }
       
           
        }

              //Valv
        if(selFlag == 2){
          // only fire if button & no yerr shot is active
          if((now & 0x04) && msp.life == 0){
            if (firstFire == 0)
            Sound("valvLP.bin");
            firstFire = 1;
            msp.life = 1;
            msp.x    = XData + (SPRITE_WIDTH/2) - (msp.w/2);
            msp.y    = YData - SPRITE_HEIGHT - msp.h + 4;
            msp.vx   = 0;
            if (valvFlag == 1 ) msp.vy = -9;  
            else msp.vy   = -4;        // adjust speed if you like
            FillProjectile(msp.x, msp.y,msp.w,msp.h);
          }
          // erase old & move
          if(msp.life){
            oldProjX = msp.x;
            oldProjY = msp.y;
            msp.x += msp.vx;
            msp.y += msp.vy;
            if((msp.y + msp.h) < 0){
              msp.life = 0;
            }
            eraseOnProjectile(oldProjX, oldProjY,msp.w,msp.h);
          }
          // draw new
          if(msp.life){
            FillProjectile(msp.x, msp.y,msp.w,msp.h);
            DrawProjectileWithTransparency(
              msp.x, msp.y,
              msp.image,
              msp.w, msp.h
            );
          }
          if (now == 2 && g2flag == 1) {
            g2act = Time;
            g2flag = 0;
            valvFlag = 1;
          }
          if (valvFlag == 1) {
          if ((Time - g2act) >=  4*30) {
            valvFlag = 0;
            }
          }
          
        }

        // --- Holt’s mustache attack (now animated) ---
        if(selFlag == 1){
          if (firstFire == 0) {
            Sound("hmust.bin");
            firstFire = 1;
          }
          // fire on button press, if no shot already active
          // if (now == 1) {
          //   Sound("hShave.bin");
          // }
          if (now == 2) {Sound ("hprith.bin");Clock_Delay1ms(10);}
          if (now == 8) {Sound ("hatt.bin");Clock_Delay1ms(10);}
          if((now & 0x04) && mustShot.life == 0){
            mustShot.life = 1;
            mustShot.x    = XData + (SPRITE_WIDTH/2) - (mustShot.w/2);
            mustShot.y    = YData - SPRITE_HEIGHT - mustShot.h;
            mustShot.vx   = 0;
            mustShot.vy   = -6;
            // draw an orange box behind it
            FillProjectile(mustShot.x, mustShot.y,mustShot.w,mustShot.h);
            // reset animation
            mustFrameIndex     = 0;
            mustShot.image     = mustFrames[0];
          }
          // erase old & move
          if(mustShot.life){
            int16_t oldProjX = mustShot.x;
            int16_t oldProjY = mustShot.y;
            mustShot.x += mustShot.vx;
            mustShot.y += mustShot.vy;
            // if it leaves screen, deactivate
            if((mustShot.y + mustShot.h) < 0){
              mustShot.life = 0;
            }
            // clear its old box
            ST7735_FillRect(oldProjX,
                            oldProjY,
                            mustShot.w,
                            mustShot.h,
                            BACKGROUND_COLOR);
          }
          // draw new with cycling frames
          if(mustShot.life){
            mustFrameIndex = (mustFrameIndex + 1) % MUSTACHE_FRAMES;
            mustShot.image = mustFrames[mustFrameIndex];
            DrawProjectileWithTransparency(
              mustShot.x, mustShot.y,
              mustShot.image,
              mustShot.w, mustShot.h
            );
          }
        }

        // --- 4) Projectile ↔ Enemy collision & damage (3 bots) ---
        for(int i = 0; i < 3; ++i){
          if(!enemyBot[i].life) continue;
          // enemy box
          int ex1 = enemyBot[i].x;
          int ey1 = enemyBot[i].y;
          int ex2 = ex1 + enemyBot[i].w - 1;
          int ey2 = ey1 + enemyBot[i].h - 1;
          // helper to test overlap
          auto hit = [&](Sprite *p){
            if(!p->life) return false;
            int px1 = p->x;
            int py1 = p->y;
            int px2 = px1 + p->w - 1;
            int py2 = py1 + p->h - 1;
            return !(px2 < ex1 || px1 > ex2 || py2 < ey1 || py1 > ey2);
          };
          // mustShot
          if(hit(&mustShot)){
            ST7735_FillRect(mustShot.x, mustShot.y, mustShot.w, mustShot.h, BACKGROUND_COLOR);
            mustShot.life = 0;
            enemyHealth[i] = (enemyHealth[i] > DMG_MUST ? enemyHealth[i] - DMG_MUST : 0);
          }
          // deref
          if(hit(&deref)){
            ST7735_FillRect(deref.x, deref.y, deref.w, deref.h, BACKGROUND_COLOR);
            deref.life = 0;
            enemyHealth[i] = (enemyHealth[i] > DMG_DEREF ? enemyHealth[i] - DMG_DEREF : 0);
          }
          // pointer
          if(hit(&point)){
            ST7735_FillRect(point.x, point.y, point.w, point.h, BACKGROUND_COLOR);
            point.life = 0;
            enemyHealth[i] = (enemyHealth[i] > DMG_POINT ? enemyHealth[i] - DMG_POINT : 0);
          }

          // yint (Yerraballi’s special) collision & freeze
          if(hit(&yint)){
            // clear yint
            ST7735_FillRect(yint.x, yint.y, yint.w, yint.h, BACKGROUND_COLOR);
            // apply reduced damage
            enemyHealth[i] = (enemyHealth[i] > YINT_DAMAGE ? enemyHealth[i] - YINT_DAMAGE : 0);
            // set freeze timer
            freezeEndTime = Time + FREEZE_TICKS;
          }

          // if that kill landed, handle death/respawn immediately:
          if(enemyHealth[i] == 0){
            score += 10;
            killCount++;

            if ((killCount % 3) == 0) {
              // bump speed level
              enemySpeedLevel++;
              // optionally cap it so it doesn’t get absurd
              if (enemySpeedLevel > 10) enemySpeedLevel = 10;
            }

            // clear corpse
            ST7735_FillRect(ex1, ey1, enemyBot[i].w, enemyBot[i].h, BACKGROUND_COLOR);
            // speed up
            if(enemyBot[i].vx>0) enemyBot[i].vx++; else enemyBot[i].vx--;
            if(enemyBot[i].vy>0) enemyBot[i].vy++; else if(enemyBot[i].vy<0) enemyBot[i].vy--; else enemyBot[i].vy = 1;
            // clamp
            if(enemyBot[i].vx>10)   enemyBot[i].vx=10;
            if(enemyBot[i].vx<-10)  enemyBot[i].vx=-10;
            if(enemyBot[i].vy>10)   enemyBot[i].vy=10;
            if(enemyBot[i].vy<-10)  enemyBot[i].vy=-10;
            // reset health & life
            enemyHealth[i]   = ENEMY_MAX_HEALTH;
            enemyBot[i].life = 1;
            // respawn at top
            enemyBot[i].x = Random(SCREEN_WIDTH - enemyBot[i].w);
            enemyBot[i].y = 0;
          }
        }

        // … inside your hit-and-respawn loop …
        for(int i = 0; i < 3; i++){
          if(!enemyBot[i].life) continue;
          // collision check with mustShot/deref/point…
          // if enemyHealth[i] hits 0:
          if(enemyHealth[i] == 0){
            score += 10;
            killCount++;
            // clear corpse
            ST7735_FillRect(enemyBot[i].x, enemyBot[i].y,
                            enemyBot[i].w, enemyBot[i].h,
                            BACKGROUND_COLOR);
            // increase speed, clamp, reset health & life
            if(enemyBot[i].vx > 0) enemyBot[i].vx++;
            else                  enemyBot[i].vx--;
            if(enemyBot[i].vy > 0) enemyBot[i].vy++;
            else if(enemyBot[i].vy < 0) enemyBot[i].vy--;
            else enemyBot[i].vy = 1;
            if(enemyBot[i].vx > 10)  enemyBot[i].vx = 10;
            if(enemyBot[i].vx < -10) enemyBot[i].vx = -10;
            if(enemyBot[i].vy > 10)  enemyBot[i].vy = 10;
            if(enemyBot[i].vy < -10) enemyBot[i].vy = -10;
            enemyHealth[i] = ENEMY_MAX_HEALTH;
            enemyBot[i].life = 1;
            // respawn at top
            enemyBot[i].x = Random(SCREEN_WIDTH - enemyBot[i].w);
            enemyBot[i].y = 0;
          }
        }

        // unlock bot #2 at  5 kills
        if(killCount >=  5) enemyBot[1].life = 1;
        // unlock bot #3 at 10 kills
        if(killCount >= 10) enemyBot[2].life = 1;

        // --- 5) Player ↔ Enemy collision: damage/shield + mutual knockback (3 bots) ---
        for(int i = 0; i < 3; ++i){
          if(!enemyBot[i].life) continue;
          // Player AABB
          int px1 = XData;
          int py1 = YData - SPRITE_HEIGHT + 1;
          int px2 = XData + SPRITE_WIDTH;
          int py2 = YData;
          // Enemy i AABB
          int ex1 = enemyBot[i].x;
          int ey1 = enemyBot[i].y;
          int ex2 = ex1 + enemyBot[i].w;
          int ey2 = ey1 + enemyBot[i].h;
          // overlap?
          if(!(px1 > ex2 || px2 < ex1 || py1 > ey2 || py2 < ey1)){
            // --- KNOCKBACK ---
            // compute center‐to‐center vector
            int playerCX = XData + SPRITE_WIDTH/2;
            int playerCY = YData - SPRITE_HEIGHT/2;
            int enemyCX  = ex1 + enemyBot[i].w/2;
            int enemyCY  = ey1 + enemyBot[i].h/2;
            int dx = playerCX - enemyCX;
            int dy = playerCY - enemyCY;
            int sx = (dx >= 0 ? 1 : -1);
            int sy = (dy >= 0 ? 1 : -1);
            const int KNOCKBACK = 10;
            // move player away
            XData         = CLAMP(XData       + sx * KNOCKBACK,
                                   0, SCREEN_WIDTH  - SPRITE_WIDTH);
            YData         = CLAMP(YData       + sy * KNOCKBACK,
                                   SPRITE_HEIGHT, SCREEN_HEIGHT);
            // move enemy away
            enemyBot[i].x = CLAMP(ex1 - sx * KNOCKBACK,
                                   0, SCREEN_WIDTH  - enemyBot[i].w);
            enemyBot[i].y = CLAMP(ey1 - sy * KNOCKBACK,
                                   0, SCREEN_HEIGHT - enemyBot[i].h);
            // clear old draw
            ST7735_FillRect(px1, py1,
                            SPRITE_WIDTH, SPRITE_HEIGHT,
                            BACKGROUND_COLOR);
            ST7735_FillRect(ex1, ey1,
                            enemyBot[i].w, enemyBot[i].h,
                            BACKGROUND_COLOR);

            // --- SHIELD & DAMAGE ---
            if(selFlag == 4 && borderOn){
              // Yerr’s shield absorbs one hit
              borderOn = false;
            } else {
              // take one damage
              if(playerHealth > 0){
                playerHealth--;
                // blink RED LED for feedback
                LED_On(REDL, 0);
                Clock_Delay1ms(50);
                LED_Off(REDL, 0);
              }
              // game over if health zero
              if(playerHealth == 0 && score < 0){
                ST7735_FillScreen(ST7735_BLACK);
                ST7735_SetCursor(3,5);
                if (langFlag == 0){
                ST7735_OutString("Game Over");
                ST7735_SetCursor(3,7);
                char buf[16];
                snprintf(buf,sizeof(buf),"Score: %u",score);
                ST7735_OutString(buf);
                } else {
                ST7735_OutString("Fin de la partida");
                ST7735_SetCursor(3,7);
                char buf[16];
                snprintf(buf,sizeof(buf),"puntuaci\xC3n: %u",score);
                ST7735_OutString(buf);
                }
                while(1) {}  // halt
              } else if (playerHealth == 0){
               ST7735_FillScreen(ST7735_BLACK);
               DrawGO(selFlag); 
               ST7735_SetCursor(0,0);
               char buf[16];
                snprintf(buf,sizeof(buf),"Score: %u",score);
                ST7735_OutString(buf);
                if (selFlag == 1) Sound("hVic.bin");
                if (selFlag == 2) Sound("valvVic.bin");
                if (selFlag == 4) Sound("yVic.bin");
                if (selFlag == 8) Sound("spByte.bin");

                now = 0;
                if (now == 4) {
                  joyFlag = 0;
                  charFlag = 1;
                  music = 0;
                  drawn = 0;
                }
                while(1) {}  // halt
              }
            }



            // --- REDRAW at new positions ---
            DrawCurrent(XData, YData);
            DrawSpriteWithTransparency(
              enemyBot[i].x,
              enemyBot[i].y,
              enemyBot[i].image,
              enemyBot[i].w,
              enemyBot[i].h
            );
          }
        }
        Clock_Delay(T33ms);
      }
    }
    loopBg();
  }
}