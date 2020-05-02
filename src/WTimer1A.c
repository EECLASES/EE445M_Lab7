// WTIMER1A.c
// Runs on LM4F120/TM4C123
// Use WTIMER1 in 64-bit periodic mode to request interrupts at a periodic rate
// Jonathan Valvano
// Jan 9, 2020

/* This example accompanies the book
   "Embedded Systems: Real Time Interfacing to Arm Cortex M Microcontrollers",
   ISBN: 978-1463590154, Jonathan Valvano, copyright (c) 2020
  Program 7.5, example 7.6

 Copyright 2020 by Jonathan W. Valvano, valvano@mail.utexas.edu
    You may use, edit, run or distribute this file
    as long as the above copyright notice remains
 THIS SOFTWARE IS PROVIDED "AS IS".  NO WARRANTIES, WHETHER EXPRESS, IMPLIED
 OR STATUTORY, INCLUDING, BUT NOT LIMITED TO, IMPLIED WARRANTIES OF
 MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE APPLY TO THIS SOFTWARE.
 VALVANO SHALL NOT, IN ANY CIRCUMSTANCES, BE LIABLE FOR SPECIAL, INCIDENTAL,
 OR CONSEQUENTIAL DAMAGES, FOR ANY REASON WHATSOEVER.
 For more information about my classes, my research, and my books, see
 http://users.ece.utexas.edu/~valvano/
 */
#include <stdint.h>
#include "../inc/tm4c123gh6pm.h"

void (*WidePeriodicTask1)(void);   // user function

// ***************** WideTimer0A_Init ****************
// Activate WideTimer0 interrupts to run user task periodically
// Inputs:  task is a pointer to a user function
//          period in units (1/clockfreq)
//          priority 0 (highest) to 7 (lowest)
// Outputs: none
void WideTimer1A_Init(void(*task)(void), uint32_t period, uint32_t priority){
	
  SYSCTL_RCGCWTIMER_R |= 0x02;   // 0) activate WTIMER1
  WidePeriodicTask1 = task;      // user function
  WTIMER1_CTL_R &= ~0x00000001;;    // 1) disable WTIMER1A during setup
  WTIMER1_CFG_R = 0x00000004;;    // 2) configure for 32-bit mode
  WTIMER1_TAMR_R = 0x00000002;   // 3) configure for periodic mode, default down-count settings
  WTIMER1_TAILR_R = period-1;    // 4) reload value
	//WTIMER1_TBILR_R = 0;           // bits 63:32
  WTIMER1_TAPR_R = 0;            // 5) bus clock resolution
  WTIMER1_ICR_R = 0x00000001;    // 6) clear WTIMER1A timeout flag TATORIS
  WTIMER1_IMR_R |= 0x00000001;    // 7) arm timeout interrupt
	NVIC_PRI24_R = (NVIC_PRI24_R&0xFFFFFF00)|(priority<<5); // priority 
	
// interrupts enabled in the main program after all devices initialized
// vector number 112, interrupt number 96
  NVIC_EN3_R |= 0x00000001;      // 9) enable IRQ 96 in NVIC //pg 104 of datasheet
  WTIMER1_CTL_R |= 0x00000001;    // 10) enable WTIMER1A
}

void WideTimer1A_Handler(void){
  WTIMER1_ICR_R = TIMER_ICR_TATOCINT;// acknowledge WTIMER5A timeout
  (*WidePeriodicTask1)();            // execute user task
}
void WideTimer1_Stop(void){
  NVIC_DIS2_R = 1<<30;          // 9) disable interrupt 94 in NVIC
  WTIMER1_CTL_R = 0x00000000;    // 10) disable WTIMER1A
}
