/*------------------------------------------------------------------------*/
/* STM32F100: MMCv3/SDv1/SDv2 (SPI mode) control module                   */
/*------------------------------------------------------------------------*/
/*
/  Copyright (C) 2014, ChaN, all right reserved.
/
/ * This software is a free software and there is NO WARRANTY.
/ * No restriction on use. You can use, modify and redistribute it for
/   personal, non-profit or commercial products UNDER YOUR RESPONSIBILITY.
/ * Redistributions of source code must retain the above copyright notice.
/
/-------------------------------------------------------------------------*/
// converted to MSPM0
// April 11, 2024

#include <stdint.h>
#include <ti/devices/msp/msp.h>
#include "../inc/LaunchPad.h"
#include "../inc/Clock.h"
#include "../inc/Timer.h"
#include "../inc/SPI.h"
#include "integer.h"
#include "diskio.h"
#define SDC_CS_PA12 1


// hardware connections
// **********ST7735 TFT and SDC*******************
// ST7735
// Backlight (pin 10)     to +3.3 V
// MISO (pin 9)           to SPI1 POCI: PB7
// SCK (pin 8)            to SPI1 SCLK: PB9
// MOSI (pin 7)           to SPI1 PICO: PB8
// TFT_CS (pin 6)         to GPIO:  PB6
// CARD_CS (pin 5)        to PA12 (GPIO)
// Data/Command (pin 4)   to PA13 (GPIO), high for data, low for command
// RESET (pin 3)          to PB15 (GPIO)
// VCC (pin 2)            to +3.3 V
// Gnd (pin 1)            to ground


// **********HiLetgo ST7735 TFT and SDC (SDC not tested)*******************
// ST7735
// LED-   (pin 16) TFT, to ground
// LED+   (pin 15) TFT, to +3.3 V
// SD_CS  (pin 14) SDC, to PA12 chip select
// MOSI   (pin 13) SDC, to PB8 MOSI
// MISO   (pin 12) SDC, to PB7 MISO
// SCK    (pin 11) SDC, to serial clock
// CS     (pin 10) TFT, to PB6  GPIO
// SCL    (pin 9)  TFT, to PB9  SPI1 SCLK
// SDA    (pin 8)  TFT, to PB8  MOSI SPI1 PICO
// A0     (pin 7)  TFT, to PA13 Data/Command, high for data, low for command
// RESET  (pin 6)  TFT, to PB15 reset (GPIO), low to reset
// NC     (pins 3,4,5)
// VCC    (pin 2)       to +3.3 V
// GND    (pin 1)       to ground

#define TFT_CS_LOW()     (GPIOB->DOUTCLR31_0 = (1<<6))   // PB6 low
#define TFT_CS_HIGH()    (GPIOB->DOUTSET31_0 = (1<<6))   // PB6 high

// PA12 output used for SDC CS
#if SDC_CS_PA12
#define SDC_CS           GPIOA
#define SDC_CS_PIN       (1<<12)         // CS controlled by software
#define SDC_CS_LOW()     (SDC_CS->DOUTCLR31_0 = SDC_CS_PIN)   // PA12 low
#define SDC_CS_HIGH()    (SDC_CS->DOUTSET31_0 = SDC_CS_PIN)   // PA12 high
void CS_Init(void){
  IOMUX->SECCFG.PINCM[PA12INDEX]  = (unsigned long) 0x00000081;
  SDC_CS->DOE31_0 |= SDC_CS_PIN;
  SDC_CS_HIGH(); // PA12=1
}
#endif
//********SPI1_Init*****************
// Initialize SPI1 interface to SDC
// inputs: none
// outputs: none
// outputBitRate = (spiInputClock) / ((1 + SCR) * 2)
// 99 for   400,000 bps slow mode, used during initialization
// 4  for 8,000,000 bps fast mode, used during disk I/O
int sdInitialize = 0;
int LCDresetFlag;
void SPI1_Init(void){
  if (sdInitialize == 1) return;
  sdInitialize = 1;
  TimerG6_IntArm(1000,80,1);            // initialize TimerG6 for 1 ms interrupts
  CS_Init();                            // initialize whichever GPIO pin is CS for the SD card
  uint32_t busfreq =  Clock_Freq();
      // assumes GPIOA and GPIOB are reset and powered previously
      // RSTCLR to SPI1 peripherals
      //   bits 31-24 unlock key 0xB1
      //   bit 1 is Clear reset sticky bit
      //   bit 0 is reset gpio port
  SPI1->GPRCM.RSTCTL = 0xB1000003;
      // Enable power to SPI1 peripherals
      // PWREN
      //   bits 31-24 unlock key 0x26
      //   bit 0 is Enable Power
  SPI1->GPRCM.PWREN = 0x26000001;
    // configure PB9 PB6 PB8 as alternate SPI1 function
  IOMUX->SECCFG.PINCM[PB9INDEX]  = 0x00000083;  // SPI1 SCLK
  //  IOMUX->SECCFG.PINCM[PB6INDEX]  = 0x00000083;  // SPI1 CS0
  IOMUX->SECCFG.PINCM[PB8INDEX]  = 0x00000083;  // SPI1 PICO
  IOMUX->SECCFG.PINCM[PB15INDEX] = 0x00000081;  // GPIO output, LCD !RST
  IOMUX->SECCFG.PINCM[PA13INDEX] = 0x00000081;  // GPIO output, LCD RS
  IOMUX->SECCFG.PINCM[PB7INDEX]  = 0x00040083;  // SPI1 POCI
  IOMUX->SECCFG.PINCM[PB6INDEX]  = 0x00000081;  // PB6 is regular GPIO
  Clock_Delay(24); // time for gpio to power up
  GPIOA->DOE31_0 |= 1<<13;    // PA13 is LCD RS
  GPIOB->DOE31_0 |= 1<<15;    // PB15 is LCD !RST
  GPIOA->DOUTSET31_0 = 1<<13; // RS=1
  GPIOB->DOUTSET31_0 = 1<<15; // !RST = 1
  GPIOB->DOE31_0 |= 1<<6;     // PB6 is regular GPIO
  SPI1->CLKSEL = 8; // SYSCLK
  // bit 3 SYSCLK
  // bit 2 MFCLK
  // bit 1 LFCLK
  SPI1->CLKDIV = 0; // divide by 1
  // bits 2-0 n (0 to 7), divide by n+1
  //Set the bit rate clock divider to generate the serial output clock
  //     outputBitRate = (spiInputClock) / ((1 + SCR) * 2)
  //     8,000,000 = (16,000,000)/((0 + 1) * 2)
  //     8,000,000 = (32,000,000)/((1 + 1) * 2)
  //     6,666,667 = (40,000,000)/((2 + 1) * 2)
  //    10,000,000 = (40,000,000)/((1 + 1) * 2)
  //     8,000,000 = (80,000,000)/((4 + 1) * 2)
  //     8,000,000 = (Clock_Freq)/((m + 1) * 2)
  //     m = (Clock_Freq/16000000) - 1
  if(busfreq <= 16000000){
    SPI1->CLKCTL = 0; // frequency= busfreq/2
  }else if(busfreq == 40000000){
    SPI1->CLKCTL = 1; // frequency= 10MHz
     // SPI1->CLKCTL = 2; // frequency= 6.66MHz
  }else{
    SPI1->CLKCTL = busfreq/16000000 -1; // 8 MHz
   }
   SPI1->CTL0 = 0x0007;
  // bit 14 CSCLR=0 not cleared
  // bits 13-12 CSSEL=0 CS0
  // bit 9 SPH = 0
  // bit 8 SPO = 0
  // bits 6-5 FRF = 00 (3 wire)
  // bits 4-0 n=7, data size is n+1 (8bit data)
    SPI1->CTL1 = 0x0015;
  // bits 29-24 RXTIMEOUT=0
  // bits 23-16 REPEATX=0 disabled
  // bits 15-12 CDMODE=0 manual
  // bit 11 CDENABLE=0 CS3
  // bit 7-5 =0 no parity
  // bit 4=1 MSB first
  // bit 3=0 POD (not used, not peripheral)
  // bit 2=1 CP controller mode
  // bit 1=0 LBM disable loop back
  // bit 0=1 enable SPI
  TFT_CS_HIGH();             // disable LCD
  if(LCDresetFlag) return;
  LCDresetFlag = 1;
  SPI_Reset();


}


// outputBitRate = (spiInputClock) / ((1 + SCR) * 2)
// 99 for   400,000 bps slow mode, used during initialization
// 4  for 8,000,000 bps fast mode, used during disk I/O
#define FCLK_SLOW() { SPI1->CLKCTL = 99; }
#define FCLK_FAST() { SPI1->CLKCTL = 80/16 -1; }// 8 MHz


//#define  MMC_CD    !(GPIOC_IDR & _BV(4))  /* Card detect (yes:true, no:false, default:true) */
#define  MMC_CD    1  /* Card detect (yes:true, no:false, default:true) */
#define  MMC_WP    0 /* Write protected (yes:true, no:false, default:false) */
//#define  SPIx_CR1  SPI1_CR1
//#define  SPIx_SR    SPI1_SR
//#define  SPIx_DR    SPI1_DR
#define  SPIxENABLE() {SPI1_Init();}

/*--------------------------------------------------------------------------

   Module Private Functions

---------------------------------------------------------------------------*/
/* MMC/SD command */
#define CMD0   (0)      /* GO_IDLE_STATE */
#define CMD1   (1)      /* SEND_OP_COND (MMC) */
#define ACMD41 (0x80+41)  /* SEND_OP_COND (SDC) */
#define CMD8   (8)     /* SEND_IF_COND */
#define CMD9   (9)     /* SEND_CSD */
#define CMD10  (10)    /* SEND_CID */
#define CMD12  (12)    /* STOP_TRANSMISSION */
#define ACMD13 (0x80+13)  /* SD_STATUS (SDC) */
#define CMD16  (16)    /* SET_BLOCKLEN */
#define CMD17  (17)    /* READ_SINGLE_BLOCK */
#define CMD18  (18)    /* READ_MULTIPLE_BLOCK */
#define CMD23  (23)    /* SET_BLOCK_COUNT (MMC) */
#define ACMD23 (0x80+23)  /* SET_WR_BLK_ERASE_COUNT (SDC) */
#define CMD24  (24)    /* WRITE_BLOCK */
#define CMD25  (25)    /* WRITE_MULTIPLE_BLOCK */
#define CMD32  (32)    /* ERASE_ER_BLK_START */
#define CMD33  (33)    /* ERASE_ER_BLK_END */
#define CMD38  (38)    /* ERASE */
#define CMD55  (55)    /* APP_CMD */
#define CMD58  (58)    /* READ_OCR */

static volatile DSTATUS Stat = STA_NOINIT;  /* Physical drive status */

static volatile UINT Timer1, Timer2;  /* 1kHz decrement timer stopped at zero (disk_timerproc()) */

static BYTE CardType;      /* Card type flags */



/*-----------------------------------------------------------------------*/
/* SPI controls (Platform dependent)                                     */
/*-----------------------------------------------------------------------*/

/* Initialize MMC interface */
static void init_spi(void){
  SPIxENABLE();    /* Enable SPI function */
  SDC_CS_HIGH();   /* Set CS# high */

  for (Timer1 = 10; Timer1; ) ;  /* 10ms */
}


/* Exchange a byte */
// Inputs:  byte to be sent to SPI
// Outputs: byte received from SPI
// assumes it has been selected with CS low
static BYTE xchg_spi(BYTE data){ BYTE volatile rcvdat;
// wait until SPI1 not busy/
  while((SPI1->STAT&0x10) == 0x10){}; // spin SPI busy
  SPI1->TXDATA = data;
  while((SPI1->STAT&0x04) == 0x04){}; // spin SPI RxFifo empty
  rcvdat = SPI1->RXDATA; // acknowledge response
//  while((SSI0_SR_R&SSI_SR_BSY)==SSI_SR_BSY){};
//  SSI0_DR_R = dat;                      // data out
//  while((SSI0_SR_R&SSI_SR_RNE)==0){};   // wait until response
//  rcvdat = SSI0_DR_R;                   // acknowledge response
  return rcvdat;
}

/*-----------------------------------------------------------------------*/
/* Receive a byte from MMC via SPI  (Platform dependent)                 */
/*-----------------------------------------------------------------------*/
// Inputs:  none
// Outputs: byte received from SPI
// assumes it has been selected with CS low
static BYTE rcvr_spi(void){
    // wait until SPI1 not busy/
   while((SPI1->STAT&0x10) == 0x10){}; // spin SPI busy
   SPI1->TXDATA = 0xFF; // data out, garbage
   while((SPI1->STAT&0x04) == 0x04){}; // spin SPI RxFifo empty
   return (BYTE)SPI1->RXDATA; // read received data

//  while((SSI0_SR_R&SSI_SR_BSY)==SSI_SR_BSY){};
//  SSI0_DR_R = 0xFF;                     // data out, garbage
//  while((SSI0_SR_R&SSI_SR_RNE)==0){};   // wait until response
//  return (BYTE)SSI0_DR_R;               // read received data
}

/* Receive multiple byte */
// Input:  buff Pointer to empty buffer into which data will be received
//         btr  Number of bytes to receive (even number)
// Output: none
static void rcvr_spi_multi(BYTE *buff, UINT btr){
  while(btr){
    *buff = rcvr_spi();   // return by reference
    btr--; buff++;
  }
}


#if _USE_WRITE
/* Send multiple byte */
// Input:  buff Pointer to the data which will be sent
//         btx  Number of bytes to send (even number)
// Output: none
static void xmit_spi_multi(const BYTE *buff, UINT btx){
  BYTE volatile rcvdat;
  while(btx){
    SPI1->TXDATA = *buff;
    while((SPI1->STAT&0x04) == 0x04){}; // spin SPI RxFifo empty
    rcvdat = SPI1->RXDATA; // acknowledge response
//    SSI0_DR_R = *buff;                  // data out
//    while((SSI0_SR_R&SSI_SR_RNE)==0){}; // wait until response
//    rcvdat = SSI0_DR_R;                 // acknowledge response
    btx--; buff++;
  }
}
#endif


/*-----------------------------------------------------------------------*/
/* Wait for card ready                                                   */
/*-----------------------------------------------------------------------*/
// Input:  time to wait in ms
// Output: 1:Ready, 0:Timeout
static int wait_ready(UINT wt){
  BYTE d;
  Timer2 = wt;
  do {
    d = xchg_spi(0xFF);
    /* This loop takes a time. Insert rot_rdq() here for multitask environment. */
  } while (d != 0xFF && Timer2);  /* Wait for card goes ready or timeout */
  return (d == 0xFF) ? 1 : 0;
}



/*-----------------------------------------------------------------------*/
/* Deselect card and release SPI                                         */
/*-----------------------------------------------------------------------*/
static void deselect(void){
  SDC_CS_HIGH();       /* CS = H */
  xchg_spi(0xFF);  /* Dummy clock (force DO hi-z for multiple slave SPI) */
}



/*-----------------------------------------------------------------------*/
/* Select card and wait for ready                                        */
/*-----------------------------------------------------------------------*/
// Input:  none
// Output: 1:OK, 0:Timeout in 500ms
static int select(void){
  TFT_CS_HIGH(); // make sure TFT is off
  SDC_CS_LOW();
  xchg_spi(0xFF);  /* Dummy clock (force DO enabled) */
  if(wait_ready(500)) return 1;  /* OK */
  deselect();
  return 0;  /* Timeout */
}



/*-----------------------------------------------------------------------*/
/* Receive a data packet from the MMC                                    */
/*-----------------------------------------------------------------------*/
// Input:  buff Pointer to empty buffer into which data will be received
//         btr  Number of bytes to receive (even number)
// Output: 1:OK, 0:Error on timeout
static int rcvr_datablock(BYTE *buff, UINT btr){
  BYTE token;
  Timer1 = 200;
  do {              /* Wait for DataStart token in timeout of 200ms */
    token = xchg_spi(0xFF);
    /* This loop will take a time. Insert rot_rdq() here for multitask envilonment. */
  } while ((token == 0xFF) && Timer1);
  if(token != 0xFE) return 0;    /* Function fails if invalid DataStart token or timeout */

  rcvr_spi_multi(buff, btr);    /* Store trailing data to the buffer */
  xchg_spi(0xFF); xchg_spi(0xFF);      /* Discard CRC */
  return 1;            /* Function succeeded */
}



/*-----------------------------------------------------------------------*/
/* Send a data packet to the MMC                                         */
/*-----------------------------------------------------------------------*/

#if _USE_WRITE
// Input:  buff Pointer to 512 byte data which will be sent
//         token  Token
// Output: 1:OK, 0:Failed on timeout
static int xmit_datablock(const BYTE *buff, BYTE token){
  BYTE resp;
  if (!wait_ready(500)) return 0;    /* Wait for card ready */

  xchg_spi(token);                   /* Send token */
  if (token != 0xFD) {               /* Send data if token is other than StopTran */
    xmit_spi_multi(buff, 512);       /* Data */
    xchg_spi(0xFF); xchg_spi(0xFF);  /* Dummy CRC */

    resp = xchg_spi(0xFF);        /* Receive data resp */
    if ((resp & 0x1F) != 0x05)    /* Function fails if the data packet was not accepted */
      return 0;
  }
  return 1;
}
#endif


/*-----------------------------------------------------------------------*/
/* Send a command packet to the MMC                                      */
/*-----------------------------------------------------------------------*/
// Inputs:  cmd Command index
//          arg    /* Argument
// Outputs: R1 resp (bit7==1:Failed to send)
static BYTE send_cmd(BYTE cmd, unsigned long arg){
  BYTE n, res;
  if (cmd & 0x80) {  /* Send a CMD55 prior to ACMD<n> */
    cmd &= 0x7F;
    res = send_cmd(CMD55, 0);
    if (res > 1) return res;
  }

  /* Select the card and wait for ready except to stop multiple block read */
  if (cmd != CMD12) {
    deselect();
    if (!select()) return 0xFF;
  }

  /* Send command packet */
  xchg_spi(0x40 | cmd);        /* Start + command index */
  xchg_spi((BYTE)(arg >> 24));    /* Argument[31..24] */
  xchg_spi((BYTE)(arg >> 16));    /* Argument[23..16] */
  xchg_spi((BYTE)(arg >> 8));      /* Argument[15..8] */
  xchg_spi((BYTE)arg);        /* Argument[7..0] */
  n = 0x01;              /* Dummy CRC + Stop */
  if (cmd == CMD0) n = 0x95;      /* Valid CRC for CMD0(0) */
  if (cmd == CMD8) n = 0x87;      /* Valid CRC for CMD8(0x1AA) */
  xchg_spi(n);

  /* Receive command resp */
  if (cmd == CMD12) xchg_spi(0xFF);  /* Diacard following one byte when CMD12 */
  n = 10;                /* Wait for response (10 bytes max) */
  do
    res = xchg_spi(0xFF);
  while ((res & 0x80) && --n);

  return res;              /* Return received response */
}



/*--------------------------------------------------------------------------

   Public Functions

---------------------------------------------------------------------------*/


/*-----------------------------------------------------------------------*/
/* Initialize disk drive                                                 */
/*-----------------------------------------------------------------------*/
// Inputs:  Physical drive number, which must be 0
// Outputs: status (see DSTATUS)
DSTATUS disk_initialize(BYTE drv){
  BYTE n, cmd, ty, ocr[4];

  if (drv) return STA_NOINIT;      /* Supports only drive 0 */
  init_spi();              /* Initialize SPI */

  if (Stat & STA_NODISK) return Stat;  /* Is card existing in the soket? */

  FCLK_SLOW();
  for (n = 10; n; n--) xchg_spi(0xFF);  /* Send 80 dummy clocks */

  ty = 0;
  if (send_cmd(CMD0, 0) == 1) {      /* Put the card SPI/Idle state */
    Timer1 = 1000;            /* Initialization timeout = 1 sec */
    if (send_cmd(CMD8, 0x1AA) == 1) {  /* SDv2? */
      for (n = 0; n < 4; n++) ocr[n] = xchg_spi(0xFF);  /* Get 32 bit return value of R7 resp */
      if (ocr[2] == 0x01 && ocr[3] == 0xAA) {        /* Is the card supports vcc of 2.7-3.6V? */
        while (Timer1 && send_cmd(ACMD41, 1UL << 30)) ;  /* Wait for end of initialization with ACMD41(HCS) */
        if (Timer1 && send_cmd(CMD58, 0) == 0) {    /* Check CCS bit in the OCR */
          for (n = 0; n < 4; n++) ocr[n] = xchg_spi(0xFF);
          ty = (ocr[0] & 0x40) ? CT_SD2 | CT_BLOCK : CT_SD2;  /* Card id SDv2 */
        }
      }
    } else {  /* Not SDv2 card */
      if (send_cmd(ACMD41, 0) <= 1)   {  /* SDv1 or MMC? */
        ty = CT_SD1; cmd = ACMD41;  /* SDv1 (ACMD41(0)) */
      } else {
        ty = CT_MMC; cmd = CMD1;  /* MMCv3 (CMD1(0)) */
      }
      while (Timer1 && send_cmd(cmd, 0)) ;    /* Wait for end of initialization */
      if (!Timer1 || send_cmd(CMD16, 512) != 0)  /* Set block length: 512 */
        ty = 0;
    }
  }
  CardType = ty;  /* Card type */
  deselect();
  FCLK_FAST();      /* Set fast clock */

  if (ty) {      /* OK */
    Stat &= ~STA_NOINIT;  /* Clear STA_NOINIT flag */
  } else {      /* Failed */
    Stat = STA_NOINIT;
  }

  return Stat;
}



/*-----------------------------------------------------------------------*/
/* Get disk status                                                       */
/*-----------------------------------------------------------------------*/
// Inputs:  Physical drive number, which must be 0
// Outputs: status (see DSTATUS)
DSTATUS disk_status(BYTE drv){
  if (drv) return STA_NOINIT;    /* Supports only drive 0 */
  return Stat;  /* Return disk status */
}



/*-----------------------------------------------------------------------*/
/* Read sector(s)                                                        */
/*-----------------------------------------------------------------------*/
//Inputs:  drv    Physical drive number (0)
//         buff   Pointer to the data buffer to store read data
//         sector Start sector number (LBA)
//         count  Number of sectors to read (1..128)
// Outputs: status (see DRESULT)
DRESULT disk_read(BYTE drv, BYTE *buff, unsigned long sector, UINT count){
  if (drv || !count) return RES_PARERR;    /* Check parameter */
  if (Stat & STA_NOINIT) return RES_NOTRDY;  /* Check if drive is ready */

  if (!(CardType & CT_BLOCK)) sector *= 512;  /* LBA ot BA conversion (byte addressing cards) */

  if (count == 1) {  /* Single sector read */
    if ((send_cmd(CMD17, sector) == 0)  /* READ_SINGLE_BLOCK */
      && rcvr_datablock(buff, 512))
      count = 0;
  }
  else {        /* Multiple sector read */
    if (send_cmd(CMD18, sector) == 0) {  /* READ_MULTIPLE_BLOCK */
      do {
        if (!rcvr_datablock(buff, 512)) break;
        buff += 512;
      } while (--count);
      send_cmd(CMD12, 0);        /* STOP_TRANSMISSION */
    }
  }
  deselect();

  return count ? RES_ERROR : RES_OK;  /* Return result */
}



/*-----------------------------------------------------------------------*/
/* Write sector(s)                                                       */
/*-----------------------------------------------------------------------*/

#if _USE_WRITE
//Inputs:  drv    Physical drive number (0)
//         buff   Pointer to the data buffer to write to disk
//         sector Start sector number (LBA)
//         count  Number of sectors to write (1..128)
// Outputs: status (see DRESULT)
DRESULT disk_write(BYTE drv, const BYTE *buff, unsigned long sector, UINT count){
  if (drv || !count) return RES_PARERR;    /* Check parameter */
  if (Stat & STA_NOINIT) return RES_NOTRDY;  /* Check drive status */
  if (Stat & STA_PROTECT) return RES_WRPRT;  /* Check write protect */

  if (!(CardType & CT_BLOCK)) sector *= 512;  /* LBA ==> BA conversion (byte addressing cards) */

  if (count == 1) {  /* Single sector write */
    if ((send_cmd(CMD24, sector) == 0)  /* WRITE_BLOCK */
      && xmit_datablock(buff, 0xFE))
      count = 0;
  }
  else {        /* Multiple sector write */
    if (CardType & CT_SDC) send_cmd(ACMD23, count);  /* Predefine number of sectors */
    if (send_cmd(CMD25, sector) == 0) {  /* WRITE_MULTIPLE_BLOCK */
      do {
        if (!xmit_datablock(buff, 0xFC)) break;
        buff += 512;
      } while (--count);
      if (!xmit_datablock(0, 0xFD))  /* STOP_TRAN token */
        count = 1;
    }
  }
  deselect();

  return count ? RES_ERROR : RES_OK;  /* Return result */
}
#endif


/*-----------------------------------------------------------------------*/
/* Miscellaneous drive controls other than data read/write               */
/*-----------------------------------------------------------------------*/
// Inputs:  drv,   Physical drive number (0)
//          cmd,   Control command code
//          buff   Pointer to the control data
// Outputs: status (see DRESULT)
#if _USE_IOCTL
DRESULT disk_ioctl(BYTE drv, BYTE cmd, void *buff){
  DRESULT res;
  BYTE n, csd[16];
  unsigned long *dp, st, ed, csize;


  if (drv) return RES_PARERR;          /* Check parameter */
  if (Stat & STA_NOINIT) return RES_NOTRDY;  /* Check if drive is ready */

  res = RES_ERROR;

  switch (cmd) {
  case CTRL_SYNC :    /* Wait for end of internal write process of the drive */
    if (select()) res = RES_OK;
    break;

  case GET_SECTOR_COUNT :  /* Get drive capacity in unit of sector (unsigned long) */
    if ((send_cmd(CMD9, 0) == 0) && rcvr_datablock(csd, 16)) {
      if ((csd[0] >> 6) == 1) {  /* SDC ver 2.00 */
        csize = csd[9] + ((WORD)csd[8] << 8) + ((unsigned long)(csd[7] & 63) << 16) + 1;
        *(unsigned long*)buff = csize << 10;
      } else {          /* SDC ver 1.XX or MMC ver 3 */
        n = (csd[5] & 15) + ((csd[10] & 128) >> 7) + ((csd[9] & 3) << 1) + 2;
        csize = (csd[8] >> 6) + ((WORD)csd[7] << 2) + ((WORD)(csd[6] & 3) << 10) + 1;
        *(unsigned long*)buff = csize << (n - 9);
      }
      res = RES_OK;
    }
    break;

  case GET_BLOCK_SIZE :  /* Get erase block size in unit of sector (unsigned long) */
    if (CardType & CT_SD2) {  /* SDC ver 2.00 */
      if (send_cmd(ACMD13, 0) == 0) {  /* Read SD status */
        xchg_spi(0xFF);
        if (rcvr_datablock(csd, 16)) {        /* Read partial block */
          for (n = 64 - 16; n; n--) xchg_spi(0xFF);  /* Purge trailing data */
          *(unsigned long*)buff = 16UL << (csd[10] >> 4);
          res = RES_OK;
        }
      }
    } else {          /* SDC ver 1.XX or MMC */
      if ((send_cmd(CMD9, 0) == 0) && rcvr_datablock(csd, 16)) {  /* Read CSD */
        if (CardType & CT_SD1) {  /* SDC ver 1.XX */
          *(unsigned long*)buff = (((csd[10] & 63) << 1) + ((WORD)(csd[11] & 128) >> 7) + 1) << ((csd[13] >> 6) - 1);
        } else {          /* MMC */
          *(unsigned long*)buff = ((WORD)((csd[10] & 124) >> 2) + 1) * (((csd[11] & 3) << 3) + ((csd[11] & 224) >> 5) + 1);
        }
        res = RES_OK;
      }
    }
    break;

  case CTRL_TRIM :  /* Erase a block of sectors (used when _USE_ERASE == 1) */
    if (!(CardType & CT_SDC)) break;        /* Check if the card is SDC */
    if (disk_ioctl(drv, MMC_GET_CSD, csd)) break;  /* Get CSD */
    if (!(csd[0] >> 6) && !(csd[10] & 0x40)) break;  /* Check if sector erase can be applied to the card */
    dp = (unsigned long *)buff;
    st = dp[0]; ed = dp[1];        /* Load sector block */
    if (!(CardType & CT_BLOCK)) {
      st *= 512; ed *= 512;
    }
    if (send_cmd(CMD32, st) == 0 && send_cmd(CMD33, ed) == 0 && send_cmd(CMD38, 0) == 0 && wait_ready(30000))  /* Erase sector block */
      res = RES_OK;  /* FatFs does not check result of this command */
    break;

  default:
    res = RES_PARERR;
  }

  deselect();

  return res;
}
#endif


/*-----------------------------------------------------------------------*/
/* Device timer function                                                 */
/*-----------------------------------------------------------------------*/
/* This function must be called from timer interrupt routine in period
/  of 1 ms to generate card control timing.
*/

void disk_timerproc (void)
{
  WORD n;
  BYTE s;


  n = Timer1;            /* 1kHz decrement timer stopped at 0 */
  if (n) Timer1 = --n;
  n = Timer2;
  if (n) Timer2 = --n;

  s = Stat;
  if (MMC_WP)    /* Write protected */
    s |= STA_PROTECT;
  else    /* Write enabled */
    s &= ~STA_PROTECT;
  if (MMC_CD)  /* Card is in socket */
    s &= ~STA_NODISK;
  else    /* Socket empty */
    s |= (STA_NODISK | STA_NOINIT);
  Stat = s;
}


// Executed every 1 ms
extern "C" void TIMG6_IRQHandler(void);
void TIMG6_IRQHandler(void){
  if((TIMG6->CPU_INT.IIDX) == 1){ // this will acknowledge
    disk_timerproc();
  }
}
