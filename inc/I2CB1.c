// I2CB1.c
// Runs on TM4C123
// Busy-wait device driver for the I2C0.
// Daniel and Jonathan Valvano
// Jan 4, 2020
// This file originally comes from the TIDA-010021 Firmware (tidcf48.zip) and
// was modified by Pololu to support the MSP432P401R. Modified again for TM4C123

/* This example accompanies the books
   "Embedded Systems: Introduction to ARM Cortex M Microcontrollers",
      ISBN: 978-1469998749, Jonathan Valvano, copyright (c) 2020
   "Embedded Systems: Real Time Interfacing to ARM Cortex M Microcontrollers",
      ISBN: 978-1463590154, Jonathan Valvano, copyright (c) 2020
   "Embedded Systems: Real-Time Operating Systems for ARM Cortex-M Microcontrollers",
      ISBN: 978-1466468863, Jonathan Valvano, copyright (c) 2020

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

// TM4C123 hardware
// SDA    PB3 I2C data
// SCL    PB2 I2C clock
/*
 *  I2C0 Conncection | I2C1 Conncection | I2C2 Conncection | I2C3 Conncection
 *  ---------------- | ---------------- | ---------------- | ----------------
 *  SCL -------- PB2 | SCL -------- PA6 | SCL -------- PE4 | SCL -------- PD0
 *  SDA -------- PB3 | SDA -------- PA7 | SDA -------- PE5 | SDA -------- PD1
 */
#include <stdint.h>
#include "../inc/I2CB1.h"
#include "../inc/tm4c123gh6pm.h"
#define I2C_MCS_ACK             0x00000008  // Data Acknowledge Enable
#define I2C_MCS_DATACK          0x00000008  // Acknowledge Data
#define I2C_MCS_ADRACK          0x00000004  // Acknowledge Address
#define I2C_MCS_STOP            0x00000004  // Generate STOP
#define I2C_MCS_START           0x00000002  // Generate START
#define I2C_MCS_ERROR           0x00000002  // Error
#define I2C_MCS_RUN             0x00000001  // I2C Master Enable
#define I2C_MCS_BUSY            0x00000001  // I2C Busy
#define I2C_MCR_MFE             0x00000010  // I2C Master Function Enable

// let t be bus period, let F be bus frequency
// let f be I2C frequency
// at F=80 MHz, I2C period = (TPR+1)*250ns 
// f=400kHz,    I2C period = 20*(TPR+1)*12.5ns = 2.5us, with TPR=9
// I2C period, 1/f = 20*(TPR+1)*t 
// F/f = 20*(TPR+1)
// TPR = (F/f/20)-1 
void I2C0_Init(uint32_t I2Cfreq, uint32_t busFreq){
  SYSCTL_RCGCI2C_R |= 0x0001;           // activate I2C0
  SYSCTL_RCGCGPIO_R |= 0x0002;          // activate port B
  while((SYSCTL_PRGPIO_R&0x0002) == 0){};// ready?
  GPIO_PORTB_AFSEL_R |= 0x0C;           // 3) enable alt funct on PB2,3
  GPIO_PORTB_ODR_R |= 0x08;             // 4) enable open drain on PB3 only
  GPIO_PORTB_DEN_R |= 0x0C;             // 5) enable digital I/O on PB2,3
                                        // 6) configure PB2,3 as I2C
  GPIO_PORTB_PCTL_R = (GPIO_PORTB_PCTL_R&0xFFFF00FF)+0x00003300;
  GPIO_PORTB_AMSEL_R &= ~0x0C;          // 7) disable analog functionality on PB2,3
  I2C0_MCR_R = I2C_MCR_MFE|I2C_MCR_GFE;             // 9) master function enable, glitch
  I2C0_MCR2_R = I2C_MCR2_GFPW_4; // 4 clock glitch
  I2C0_MTPR_R = ((busFreq/I2Cfreq)/20)-1; // 8) configure clock speed
}

int I2C0_Send(uint8_t slaveAddr, uint8_t *pData, uint32_t count){
  while(I2C0_MCS_R & I2C_MCS_BUSY){};                // wait for I2C ready
  I2C0_MSA_R = (slaveAddr << 1) & I2C_MSA_SA_M;      // MSA[7:1] is slave address
  I2C0_MSA_R &= ~I2C_MSA_RS;                         // MSA[0] is 0 for send
    
  if(count == 1) {
    I2C0_MDR_R = pData[0] & I2C_MDR_DATA_M;          // prepare data byte
    I2C0_MCS_R = (I2C_MCS_STOP |                     // generate stop
    
                  I2C_MCS_START |                    // generate start/restart
   
                  I2C_MCS_RUN);                      // master enable
    while (I2C0_MCS_R & I2C_MCS_BUSY){};             // wait for transmission done
    // return error bits
    return (I2C0_MCS_R&(I2C_MCS_DATACK|I2C_MCS_ADRACK|I2C_MCS_ERROR));
        
  }else {
    I2C0_MDR_R = pData[0] & I2C_MDR_DATA_M;           // prepare data byte
    I2C0_MCS_R = I2C_MCS_RUN|I2C_MCS_START;           // run and start
    while (I2C0_MCS_R & I2C_MCS_BUSY){};              // wait for transmission done
    if((I2C0_MCS_R & (I2C_MCS_DATACK|I2C_MCS_ADRACK|I2C_MCS_ERROR)) != 0){
      I2C0_MCS_R = I2C_MCS_STOP;                      // stop transmission
       // return error bits if nonzero
      return (I2C0_MCS_R & (I2C_MCS_DATACK|I2C_MCS_ADRACK|I2C_MCS_ERROR));
    }
    for(int i = 1; i < count-1; i++) {
      I2C0_MDR_R = pData[i] & I2C_MDR_DATA_M;         // prepare data byte
      I2C0_MCS_R = I2C_MCS_RUN;                       // master enable
      while (I2C0_MCS_R & I2C_MCS_BUSY){};            // wait for transmission done
      // check error bits
      if((I2C0_MCS_R & (I2C_MCS_DATACK|I2C_MCS_ADRACK|I2C_MCS_ERROR)) != 0){
        I2C0_MCS_R = I2C_MCS_STOP;                    // stop transmission
       // return error bits if nonzero
        return (I2C0_MCS_R & (I2C_MCS_DATACK|I2C_MCS_ADRACK|I2C_MCS_ERROR));
      }
    }      
    I2C0_MDR_R = pData[count-1] & I2C_MDR_DATA_M;     // prepare last byte
    I2C0_MCS_R = (I2C_MCS_STOP |                      // generate stop
                  I2C_MCS_RUN);                       // master enable
    while (I2C0_MCS_R & I2C_MCS_BUSY) {};             // wait for transmission done
    // return error bits
    return (I2C0_MCS_R & (I2C_MCS_DATACK|I2C_MCS_ADRACK|I2C_MCS_ERROR));
  }
}

int I2C0_Send4(uint8_t slaveAddr, uint8_t data1, uint8_t data2, uint8_t data3, uint8_t data4){
  while(I2C0_MCS_R & I2C_MCS_BUSY){};       // wait for I2C ready
  I2C0_MSA_R = (slaveAddr << 1);            // MSA[7:1] is slave address
                                            // MSA[0] is 0 for send
    
  I2C0_MDR_R = data1;                       // prepare data byte
  I2C0_MCS_R = I2C_MCS_RUN|I2C_MCS_START;   // run and start
  while (I2C0_MCS_R & I2C_MCS_BUSY){};      // wait for transmission done
  if((I2C0_MCS_R & (I2C_MCS_DATACK|I2C_MCS_ADRACK|I2C_MCS_ERROR)) != 0){
    I2C0_MCS_R = I2C_MCS_STOP;              // stop transmission
       // return error bits if nonzero
    return (I2C0_MCS_R & (I2C_MCS_DATACK|I2C_MCS_ADRACK|I2C_MCS_ERROR));
  }
  // second
  I2C0_MDR_R = data2;                       // prepare data byte
  I2C0_MCS_R = I2C_MCS_RUN;                 // master enable
  while (I2C0_MCS_R & I2C_MCS_BUSY){};      // wait for transmission done
      // check error bits
  if((I2C0_MCS_R & (I2C_MCS_DATACK|I2C_MCS_ADRACK|I2C_MCS_ERROR)) != 0){
    I2C0_MCS_R = I2C_MCS_STOP;              // stop transmission
       // return error bits if nonzero
    return (I2C0_MCS_R & (I2C_MCS_DATACK|I2C_MCS_ADRACK|I2C_MCS_ERROR));
  }
   // third
  I2C0_MDR_R = data3;                       // prepare data byte
  I2C0_MCS_R = I2C_MCS_RUN;                 // master enable
  while (I2C0_MCS_R & I2C_MCS_BUSY){};      // wait for transmission done
      // check error bits
  if((I2C0_MCS_R & (I2C_MCS_DATACK|I2C_MCS_ADRACK|I2C_MCS_ERROR)) != 0){
    I2C0_MCS_R = I2C_MCS_STOP;              // stop transmission
       // return error bits if nonzero
    return (I2C0_MCS_R & (I2C_MCS_DATACK|I2C_MCS_ADRACK|I2C_MCS_ERROR));
  }
   // fourth      
  I2C0_MDR_R = data4;                       // prepare last byte
  I2C0_MCS_R = (I2C_MCS_STOP |              // generate stop
                I2C_MCS_RUN);               // master enable
  while (I2C0_MCS_R & I2C_MCS_BUSY) {};     // wait for transmission done
  // return error bits
  return (I2C0_MCS_R & (I2C_MCS_DATACK|I2C_MCS_ADRACK|I2C_MCS_ERROR));
}

int I2C0_Send1(uint8_t slaveAddr, uint8_t data){
  while(I2C0_MCS_R & I2C_MCS_BUSY){};  // wait for I2C ready
  I2C0_MSA_R = (slaveAddr << 1);       // MSA[7:1] is slave address
                                       // MSA[0] is 0 for send
  I2C0_MDR_R = data;                   // prepare data byte
  I2C0_MCS_R = (I2C_MCS_STOP |         // generate stop
                I2C_MCS_START |        // generate start/restart
                I2C_MCS_RUN);          // master enable
  while (I2C0_MCS_R & I2C_MCS_BUSY){}; // wait for transmission done
    // return error bits
  return (I2C0_MCS_R&(I2C_MCS_DATACK|I2C_MCS_ADRACK|I2C_MCS_ERROR));     
}

int I2C0_Recv(uint8_t slaveAddr, uint8_t *pData, uint32_t count){
  while(I2C0_MCS_R & I2C_MCS_BUSY){};                    // wait for I2C ready
  switch(count){
    case 1:
        I2C0_MSA_R = (slaveAddr << 1) & I2C_MSA_SA_M;    // MSA[7:1] is slave address
        I2C0_MSA_R |= I2C_MSA_RS;                        // MSA[0] is 1 for receive
              
        I2C0_MCS_R = (I2C_MCS_STOP  |                    // generate stop
                      I2C_MCS_START |                    // generate start/restart
                      I2C_MCS_RUN);                      // master enable
        while (I2C0_MCS_R & I2C_MCS_BUSY) {};            // wait for transmission done
        pData[0] = (I2C0_MDR_R & I2C_MDR_DATA_M);        // usually 0xFF on error
        break;
    case 2:
        I2C0_MSA_R = (slaveAddr << 1) & I2C_MSA_SA_M;    // MSA[7:1] is slave address
        I2C0_MSA_R |= I2C_MSA_RS;                        // MSA[0] is 1 for receive
            
        I2C0_MCS_R = (I2C_MCS_ACK   |                    // positive data ack
                      I2C_MCS_START |                    // generate start/restart
                      I2C_MCS_RUN);                      // master enable
        while (I2C0_MCS_R & I2C_MCS_BUSY) {};            // wait for transmission done
        pData[0] = (I2C0_MDR_R & I2C_MDR_DATA_M);        // most significant byte
              
        I2C0_MCS_R = (I2C_MCS_STOP |                     // generate stop
                      I2C_MCS_RUN);                      // master enable
        while(I2C0_MCS_R & I2C_MCS_BUSY){};              // wait for transmission done
        pData[1] = (I2C0_MDR_R & I2C_MDR_DATA_M);        // least significant byte                                                       // repeat if error
        break;
    default:
        I2C0_MSA_R = (slaveAddr << 1) & I2C_MSA_SA_M;    // MSA[7:1] is slave address
        I2C0_MSA_R |= I2C_MSA_RS;                        // MSA[0] is 1 for receive
              
        I2C0_MCS_R = (I2C_MCS_ACK   |                    // positive data ack
                      I2C_MCS_START |                    // generate start/restart
                      I2C_MCS_RUN);                      // master enable
        while (I2C0_MCS_R & I2C_MCS_BUSY) {};            // wait for transmission done
        pData[0] = (I2C0_MDR_R & I2C_MDR_DATA_M);        // most significant byte               
        for(int i = 1; i < count-1; i++){
          I2C0_MCS_R = (I2C_MCS_ACK |                    // positive data ack
                        I2C_MCS_RUN);                    // master enable
          while (I2C0_MCS_R & I2C_MCS_BUSY) {};          // wait for transmission done
          pData[i] = (I2C0_MDR_R & I2C_MDR_DATA_M);      // read byte
        }
             
        I2C0_MCS_R = (I2C_MCS_STOP |                     // generate stop
                      I2C_MCS_RUN);                      // master enable
        while (I2C0_MCS_R & I2C_MCS_BUSY) {};            // wait for transmission done
        pData[count-1] = (I2C0_MDR_R & I2C_MDR_DATA_M);  // least significant byte
        break;
  }
  return (I2C0_MCS_R & (I2C_MCS_DATACK|I2C_MCS_ADRACK|I2C_MCS_ERROR));
}

int I2C0_Recv3(uint8_t slaveAddr, uint8_t data[3]){
  while(I2C0_MCS_R & I2C_MCS_BUSY){};         // wait for I2C ready
  I2C0_MSA_R = (slaveAddr << 1)|I2C_MSA_RS;   // MSA[7:1] is slave address
                                              // MSA[0] is 1 for receive
// first              
  I2C0_MCS_R = (I2C_MCS_ACK   |               // positive data ack
                I2C_MCS_START |               // generate start/restart
                I2C_MCS_RUN);                 // master enable
  while (I2C0_MCS_R & I2C_MCS_BUSY) {};       // wait for transmission done
  data[0] = I2C0_MDR_R ;                      // most significant byte               
 // second
  I2C0_MCS_R = (I2C_MCS_ACK |                 // positive data ack
                I2C_MCS_RUN);                 // master enable
  while (I2C0_MCS_R & I2C_MCS_BUSY) {};       // wait for transmission done
  data[1] = I2C0_MDR_R;                       // read byte
// third     
  I2C0_MCS_R = (I2C_MCS_STOP |                // generate stop
                I2C_MCS_RUN);                 // master enable
  while (I2C0_MCS_R & I2C_MCS_BUSY) {};       // wait for transmission done
  data[2] = I2C0_MDR_R;                       // least significant byte
  return (I2C0_MCS_R & (I2C_MCS_DATACK|I2C_MCS_ADRACK|I2C_MCS_ERROR));
}

