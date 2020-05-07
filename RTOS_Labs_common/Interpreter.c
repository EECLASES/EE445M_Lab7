// *************Interpreter.c**************
// Students implement this as part of EE445M/EE380L.12 Lab 1,2,3,4 
// High-level OS user interface
// 
// Runs on LM4F120/TM4C123
// Jonathan W. Valvano 1/18/20, valvano@mail.utexas.edu
#include <stdint.h>
#include <string.h> 
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "../RTOS_Labs_common/OS.h"
#include "../RTOS_Labs_common/ST7735.h"
#include "../inc/ADCT0ATrigger.h"
#include "../inc/ADCSWTrigger.h"
#include "../RTOS_Labs_common/UART0int.h"
#include "../RTOS_Labs_common/eDisk.h"
#include "../RTOS_Labs_common/eFile.h"
#include "../RTOS_Labs_common/Interpreter.h"


#include "../inc/Command.h"

#include "../RTOS_Labs_common/esp8266.h"


WifiStatus wifiStatus; 
int ServerID = -1;

//array for struct of commands
PartCommands Commands[NUMCOMMANDS] = {		
	{"help", &Help_Command},
	{"lcd_top", &Lcd_Top_Command},
	{"lcd_bot", &Lcd_Bot_Command},
	{"os_ms", &Os_Ms_Command},
	{"adc", &Adc_Command},
	{"measurements", &Measurements_Command},
	{"filesys", &Filesys_Command},
	{"load", &Load_Command},
	{"Wifi", &Wifi_Command},
	{"client", &Client_Command},
	{"led", &LED_Command}

};

//array for struct of RPC protocols
RPC_Convert RPC_Protocol[RPC_NUM] = {		
	{"help", "0"},
	{"lcd_top", "1"},
	{"lcd_bot", "2"},
	{"os_ms", "3"},
	{"adc", "4"},
//	{"measurements", "5"},
	{"filesys", "6"},
	{"load", "7"},
//	{"Wifi", "8"},
//	{"client", "9"},
	{"led", "10"},

	//os_ms (for os_time)
	{"clear", "cr5"},
	{"show", "sw4"},
	//adc
	{"sample", "se6"},
	//load  (axf files)
	{"user", "ur4"},
	//filesys
	{"start", "st5"},
	{"format", "ft6"},
	{"pdir", "pr4"},
	{"create", "ce6"},
	{"write", "we5"},
	{"pfile", "pe5"},
	{"delete", "de6"},
	{"close", "ce5"},
	//led on/off
	{"red", "rd3"},
	{"green", "gn5"},
	{"blue", "be4"},

};

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

extern int32_t MaxJitter;             // largest time jitter between interrupts in usec
extern uint32_t const JitterSize;
extern uint32_t JitterHistogram1[];

extern int32_t MaxJitter2;             // largest time jitter between interrupts in usec
extern uint32_t const JitterSize2;
extern uint32_t JitterHistogram2[];
// Print jitter histogram
void Jitter(){
  // write this for Lab 3 (the latest)
	char str[8];
	//print out to LCD
	ST7735_Message(0, 0, "MaxJitter1: ", MaxJitter);
	//print out to UART
	for(uint32_t i = 0;i < JitterSize; i++){
			sprintf(str, "%d", JitterHistogram1[i]);
			str[7] = '\n';
			UART_OutString(str);
			UART_OutString(" ");
	}
	
	UART_OutString("\r\nMaxJitter1: ");
	sprintf(str, "%d", MaxJitter);
	UART_OutString(str);
	UART_OutString("\n\r");
	
	//print out to LCD
	ST7735_Message(1, 0, "MaxJitter2: ", MaxJitter2);
	//print out to UART
	for(uint32_t i = 0;i < JitterSize2; i++){
		sprintf(str, "%d", JitterHistogram2[i]);
		str[7] = '\n';
		UART_OutString(str);
		UART_OutString(" ");
	}
	
	UART_OutString("\r\nMaxJitter2: ");
	sprintf(str, "%d", MaxJitter2);
	UART_OutString(str);
	UART_OutString("\n\r");
}


//Outstring code from labdoc
void Interpreter_OutString(char *s) {
	if(OS_Id() == ServerID) {
		if(!ESP8266_Send(strcat(s,"\n\r"))) { 
			// Error handling, close and kill 
		}
	} else {
			UART_OutString(s);
	}
} 

//Instring interpreter
void Interpreter_InString() {
	char input[40];									//does size matter much?
	if(OS_Id() == ServerID) {
		if(!ESP8266_Receive(input, 39)) { 
			// Error handling, close and kill 
		}
	} else {
		UART_InString(input, 39);
	}
} 


// *********** Command line interpreter (shell) ************
void Interpreter(void){ 
  // initialize values
	char input[40];
	char* args[MAX_ARGS];  //temp size for testing strtok 
	//initialize wifi struct
	wifiStatus.initEnable = 0;
	wifiStatus.connected = 0;
	wifiStatus.mode = 0;
	wifiStatus.MAC = 0;
	
	
	
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
				break;
			}
		}
		
		//cleans the array for future commands
		for(uint8_t i = 0; i < MAX_ARGS; i++){				//quick test code
			args[i] = NULL;
		}
	}
}

//##################Start of new functions by Miguel ###########//

/* Wifi connection  

Slave/Client side
	1. turn on wifi_switch()  --> wifi on
	2. turn on wifi_client() ---> wifi client
	3. now connected --> no command
		3a. send commands from interpreter --> client [blank] ...
		3b. if( "wifi off" ) called, close tm4c
		3c. loop #3
	

Master
	1. turn on wifi_switch()  --> wifi on
	2. turn on wifi_server() ---> wifi server
	3. waiting system to wait for tm4c()  --> no command




*/



/********Wifi_Switch********//*
Enables a thread to wait for a connection to pop up Once a semaphore gets freed	
	inputs:
			bool: true for turning on connections, false for disabling connections
	outputs: 
			bool success/true 
			
Command "Wifi on" would call this function(1);
*/
void Wifi_Switch( bool mode){
/* method:b013ea4c481621eb848fc292a1d1126c7cb110f3
	1. call init
	2a. enable if called, 
	2b. call stop server, any UART INTERFACING to turn off
*/
	
	if (mode == true){
		
		//does init (executed only once)
		if(wifiStatus.initEnable == 0){
			if(!ESP8266_Init(true,false)) {  // verbose rx echo on UART for debugging
				ST7735_DrawString(0,1,"No Wifi adapter",ST7735_YELLOW);
				
			}
			wifiStatus.initEnable = 1;


		}
		
		
		if(wifiStatus.connected == 0){	//only connect when not already connected
		
		// Get Wifi adapter version (for debugging)
		ESP8266_GetVersionNumber(); 
		// Connect to access point
		if(!ESP8266_Connect(true)) {  
			ST7735_DrawString(0,1,"No Wifi network",ST7735_YELLOW); 
		}
			ST7735_DrawString(0,1,"Wifi connected",ST7735_GREEN);
		}
		//ESP8266_StartServer(80,600); add to enable connections
		
		
	}
	else if(mode == false){
		if(wifiStatus.connected){	//only disconnect when connected
			ESP8266_CloseTCPConnection();
			wifiStatus.connected = false;
		}
	}
}
char Response1[64];

void Wifi_Client(){
	
		//does init (executed only once)
		if(wifiStatus.initEnable == 0){
			if(!ESP8266_Init(true,false)) {  // verbose rx echo on UART for debugging
				ST7735_DrawString(0,1,"No Wifi adapter",ST7735_YELLOW);
			}
			wifiStatus.initEnable = 1;
		}
		
		
		if(wifiStatus.connected == 0){	//only connect when not already connected
		
			// Get Wifi adapter version (for debugging)
			ESP8266_GetVersionNumber(); 
			// Connect to access point
			if(!ESP8266_Connect(true)) {  
				ST7735_DrawString(0,1,"No Wifi network",ST7735_YELLOW); 
			}
				ST7735_DrawString(0,1,"Wifi connected",ST7735_GREEN);
				wifiStatus.connected = true;
		}

		//assuming client is connected (add code later)
		if(!ESP8266_MakeTCPConnection("192.168.1.136", 4000, 0)){ // open socket to web server on port 80 !!!!!!!!!!!! change to other microcontroller

			ST7735_DrawString(0,2,"Connection failed",ST7735_YELLOW); 
		}    
		
		OS_Kill();
		
		//add interpreter again
		/*
		// Send request to server
  (!ESP8266_Send("hello")){
    ST7735_DrawString(0,2,"Send failed",ST7735_YELLOW); 
    ESP8266_CloseTCPConnection();
    OS_Kill();
  }    
  */
		
		//OS_Kill();
		// Send request to server
//		if(!ESP8266_Send("hello")){
//			ST7735_DrawString(0,2,"Send failed",ST7735_YELLOW); 
//			ESP8266_CloseTCPConnection();
//		}    
		// Receive response
//		if(!ESP8266_Receive(Response, 64)) {
//			ST7735_DrawString(0,2,"No response",ST7735_YELLOW); 
//			ESP8266_CloseTCPConnection();
//		}
	
	
		

}

Sema4Type ServerSema;
bool KillServer;
char WiFiResponse[64];

void ServeRequest(void){ 
  ST7735_DrawString(0,3,"Connected           ",ST7735_YELLOW); 
  ServerID = OS_Id();	//this thread calls the functions so this ID needs to be the one for wrapper to determine
	
	while(1){
  // Receive request
  if(!ESP8266_Receive(WiFiResponse, 64)){
    ST7735_DrawString(0,3,"No request",ST7735_YELLOW); 
    ESP8266_CloseTCPConnection();
    OS_Signal(&ServerSema);
    OS_Kill();
  }
    
  // check for HTTP GET
  //if(strncmp(Response, "GET", 3) == 0) 
	
	//do processing request here
	char* args[MAX_ARGS];
	
	//cleans the array
	for(uint8_t i = 0; i < MAX_ARGS; i++){				//quick test code
		args[i] = NULL;
	}
	
	//parses command and puts tokens into args array
	char* token = strtok(WiFiResponse, " ");
	for(uint8_t i = 0; token != NULL; i++){ //quick test code
			args[i] = token;
			token = strtok(NULL, " ");
	}
	
	//args should be client + (encoded) regular command prompt at this point

	if(strcmp(args[0], "client") == 0){
		int index = atoi(args[1]);
		if(index < NUMCOMMANDS){
			Commands[index].command(args);
		}
	}
	}	
	ServerID = -1;
	
	//end of processing
	
 // ESP8266_CloseTCPConnection();
	
  OS_Signal(&ServerSema);

  OS_Kill();
}

void Wifi_Server(){
	if(wifiStatus.connected == false){
		if(wifiStatus.initEnable == 0){
			// Initialize and bring up Wifi adapter  
			if(!ESP8266_Init(true,false)) {  // verbose rx echo on UART for debugging
				ST7735_DrawString(0,1,"No Wifi adapter",ST7735_YELLOW); 
				OS_Kill();
			}
			wifiStatus.initEnable = 1;
		}
		// Get Wifi adapter version (for debugging)
		ESP8266_GetVersionNumber(); 
		// Connect to access point
		if(!ESP8266_Connect(true)) {  
			ST7735_DrawString(0,1,"No Wifi network",ST7735_YELLOW); 
			OS_Kill();
		}
		wifiStatus.connected = true;
		ST7735_DrawString(0,1,"Wifi connected",ST7735_GREEN);
		if(!ESP8266_StartServer(4000,600)) {  // port 80, 5min timeout
			ST7735_DrawString(0,2,"Server failure",ST7735_YELLOW); 
			OS_Kill();
		}  
		ST7735_DrawString(0,2,"Server started",ST7735_GREEN);
		KillServer = false;
		
		OS_InitSemaphore(&ServerSema, 0);
		
		while(1) {
			// Wait for connection
			ESP8266_WaitForConnection();
			
			// Launch thread with higher priority to serve request
			OS_AddThread(&ServeRequest,128,1);	
			
			// The ESP driver only supports one connection, wait for the thread to complete
			OS_Wait(&ServerSema);
			
			if(KillServer){
				wifiStatus.connected = false;
				ESP8266_DisableServer();
				OS_Kill();		//allows server function to stop
			}
		}
	}
}

void Disconnect_Server(){
	ESP8266_CloseTCPConnection();
}
/********Tm4c_model_enable********//*
	
	inputs:
			bool: true for turning on connections, false for disabling connections
	outputs: 
			bool success/true 
*/
bool checkTm4cMode(){
return 1;


	
//: returns what mode you are on

}
bool acceptTm4c(WifiStatus *status){

//: must ping once requesting connection (probably will be respond yes to the interpreter)

return 1;

}
bool requestTm4c(WifiStatus *status, char* urlsshwhatever){


return 1;

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

/* LED_xxx()   */
// Allow remote LEDs to be toggled on or off
/* 	method:
*/
void LED_xxx(void);










