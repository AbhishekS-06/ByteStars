/*
 * LED.cpp
 *
 *  Created on: Nov 5, 2023
 *      Author:
 */
#include <cstdint>
#include <ti/devices/msp/msp.h>
#include "../inc/LaunchPad.h"
// LaunchPad.h defines all the indices into the PINCM table

// initialize your LEDs
void LED_Init(void){
    // write this
    // PINCM
    //   bit 25 is HiZ
    //   bit 20 is drive strength
    //   bit 18 is input enable control
    //   bit 17 is pull up control
    //   bit 16 is pull down control
    //   bit 7 is PC peripheral connected, enable transparent data flow
    //   bit 0 selects GPIO function
      IOMUX->SECCFG.PINCM[PA16INDEX] = (uint32_t) 0x00000081;
      IOMUX->SECCFG.PINCM[PA17INDEX] = (uint32_t) 0x00000081;
      IOMUX->SECCFG.PINCM[PB19INDEX] = (uint32_t) 0x00000081;
      IOMUX->SECCFG.PINCM[PA24INDEX] = (uint32_t) 0x00000081;
    // DOE31_0 Data output enable
      GPIOA->DOE31_0 |= (1<<16)|(1<<17)|(1<<24);
      GPIOA->DOUTCLR31_0 |= (1<<16)|(1<<17)|(1<<24); // LED1 off
      GPIOB->DOE31_0 |= (1<<19);
      GPIOB->DOUTCLR31_0 |= (1<<19);

}
// data specifies which LED to turn on
void LED_On(uint32_t a_mask, uint32_t b_mask){
  GPIOA->DOUTSET31_0 |= a_mask;  // Turn on PA LEDs
  GPIOB->DOUTSET31_0 |= b_mask;  // Turn on PB LEDs
}

// data specifies which LED to turn off
void LED_Off(uint32_t a_mask, uint32_t b_mask) {
  GPIOA->DOUTCLR31_0 |= a_mask;
  GPIOB->DOUTCLR31_0 |= b_mask;
}

// data specifies which LED to toggle
void LED_Toggle(uint32_t data){
    // write this
    // use DOUTTGL31_0 register so it does not interfere with other GPIO
  GPIOA->DOUTTGL31_0 = data;
}
