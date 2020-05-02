// *************Interpreter.c**************
// Students implement this as part of EE445M/EE380L.12 Lab 1,2,3,4 
// High-level OS user interface
// 
// Runs on LM4F120/TM4C123
// Jonathan W. Valvano 1/18/20, valvano@mail.utexas.edu
#include <stdint.h>
#include <string.h> 
#include <stdio.h>
#include <stdbool.h>

#include "../RTOS_Labs_common/OS.h"
#include "../RTOS_Labs_common/ST7735.h"
#include "../inc/ADCT0ATrigger.h"
#include "../inc/ADCSWTrigger.h"
#include "../RTOS_Labs_common/UART0int.h"
#include "../RTOS_Labs_common/eDisk.h"
#include "../RTOS_Labs_common/eFile.h"
#include "../inc/Command.h"

#include "../RTOS_Labs_common/esp8266.h"

void OutCRLF(void){
  UART_OutChar(CR);
  UART_OutChar(LF);
}

void sendMessage(char* args[5], int32_t screen, int32_t lineNumber){
	//char* message = "";
	for(int i = 0; i< 5; i++){
	//	message += args[i];
	//	message += " ";
	}
	
	//ST7735_Message(1, screen, args[i], lineNumber);

}

// Print jitter histogram
void Jitter(int32_t MaxJitter, uint32_t const JitterSize, uint32_t JitterHistogram[], uint32_t Period, uint8_t task){
  // write this for Lab 3 (the latest)
	char str[8];
	//print out to LCD
	if(task == 1){
		ST7735_Message(0, 0, "MaxJitter1: ", MaxJitter);
		for(uint32_t i = 0;i < JitterSize; i++){
			sprintf(str, "%d", JitterHistogram[i]);
			str[7] = '\n';
			UART_OutString(str);
			UART_OutString(" ");
		}
	}else{
		ST7735_Message(1, 0, "MaxJitter2: ", MaxJitter);
		for(uint32_t i = 0;i < JitterSize; i++){
			sprintf(str, "%d", JitterHistogram[i]);
			str[7] = '\n';
			UART_OutString(str);
			UART_OutString(" ");
		}
	}
	UART_OutString("\r\nMaxJitter: ");
	sprintf(str, "%d", MaxJitter);
	UART_OutString(str);
	UART_OutString("\n\r");
}


//Outstring code from labdoc
int TelnetServerID = -1;
void Interpreter_OutString(char *s) {
	if(OS_Id() == TelnetServerID) {
		if(!ESP8266_Send(s)) { 
			// Error handling, close and kill 
		}
	} else {
			UART_OutString(s);
	}
} 

//Instring interpreter
void Interpreter_InString() {
	char input[40];									//does size matter much?
	if(OS_Id() == TelnetServerID) {
		if(!ESP8266_Receive(input, 39)) { 
			// Error handling, close and kill 
		}
	} else {
		UART_InString(input, 39);
	}
} 


// *********** Command line interpreter (shell) ************
void Interpreter(void){ 
  // write this  
	char input[40];
	char* args[5];  //temp size for testing strtok 
	
	while(1){
		OutCRLF();
		UART_OutChar('-');
		UART_OutString("InString: ");
		UART_InString(input, 39);
		
		//parses command and puts tokens into args array
		char* token = strtok(input, " ");
		for(uint8_t i = 0; token != NULL; i++){ //quick test code
			args[i] = token;
			token = strtok(NULL, " ");
		}
		UART_OutString("\n\r");
		//checks first word through list of commands
		for(uint32_t i = 0; i < NUMCOMMANDS; i++){
			if(strcmp(args[0], Commands[i].trigger) == 0){
				Commands[i].command(args);
			}
		}
		
		//cleans the array for future commands
		for(uint8_t i = 0; i < 5; i++){				//quick test code
			args[i] = NULL;
		}
	}
}

//##################Start of new functions by Miguel ###########//


/********Tm4c_Mode_Enable********//*
Enables a thread to wait for a connection to pop up Once a semaphore gets freed	
	inputs:
			bool: true for turning on connections, false for disabling connections
	outputs: 
			bool success/true 
*/

void Tm4c_mode_enable(bool mode){
/* method:
	1. call init
	2a. enable if called, 
	2b. call stop server, any UART INTERFACING to turn off
*/


	if (mode == true){

	//ESP8266_EnableServer(uint16_t port);
	}
	else if(mode == false){


	}
}
// ----------ESP8266_SetServerTimeout--------------
// set connection timeout for tcp server, 0-28800 seconds
// Inputs: timeout parameter
// Outputs: 1 if success, 0 if fail


/********Tm4c_model_enable********//*
	
	inputs:
			bool: true for turning on connections, false for disabling connections
	outputs: 
			bool success/true 
*/
bool checkTm4cMode(){


	
//: returns what mode you are on

}
bool acceptTm4c(){

//: must ping once requesting connection (probably will be respond yes to the interpreter)



}
bool requestTm4c(char* urlsshwhatever){



}
/* ADC_In()   */
//Get a remote ADC sample
/* questions:
	1. how do I get a ADC sample from OS
	2. how do I make sure it has semaphores
*/
/*void ADC_In(){


}
*/



/* OS_Time()   */
//Get the time from the remote launchPad
/* method:
	1. request a remote launchpad's time 
	2. make sure you are on set to receive, then receive 
*/
//void OS_Time(){


//}


/* ST7734_Message()   */
// Output a message on the remote display
/* 	method:
	1.	make sure intrepreter gets message 
	2.	send message 
*/
void ST7734_Message();

/* LED_xxx()   */
// Allow remote LEDs to be toggled on or off
/* 	method:
*/
void LED_xxx();


/* eFile_xxx()   */
//	Remote file system access 
/* 	method:
*/
void eFile_xxx();

/* exec_elf()   */
//	Remote execute a given program on the disk	
/* 	method:
*/
void exec_elf();










