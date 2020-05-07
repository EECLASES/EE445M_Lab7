
#include <stdio.h>
#include <stdbool.h> 
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "../inc/tm4c123gh6pm.h"
#include "../inc/CortexM.h"
#include "../inc/LaunchPad.h"

#include "../RTOS_Labs_common/ST7735.h"
#include "../RTOS_Labs_common/OS.h"
#include "../RTOS_Labs_common/ADC.h"
#include "../RTOS_Labs_common/UART0int.h"
#include "../RTOS_Labs_common/eFile.h"
#include "../inc/CommandExecute.h"
#include "../RTOS_Labs_common/esp8266.h"
#include "../inc/loader.h"
#include "../RTOS_Labs_common/Interpreter.h"
#include "../inc/Command.h"

extern int ServerID;


char** RPC_Decode(char* args[]){
	if(strcmp(args[0], "client") == 0){
		for(int i = 1; i < MAX_ARGS; i++){
			for(int j = 0; j < RPC_NUM; j++){
				if(args[i] ==NULL){
					break;
				}else if(strcmp(args[i], RPC_Protocol[j].protocol) == 0){
					args[i - 1] = RPC_Protocol[j].human;	//replace client to act like regular command
				}
			}
		}
	}
	
	return args;
}

char** RPC_Encode(char* args[]){
	for(int i = 1; i < MAX_ARGS; i++){
		for(int j = 0; j < RPC_NUM; j++){
			if(strcmp(args[i], RPC_Protocol[j].human) == 0){
				args[i] = RPC_Protocol[j].protocol;
			}
		}
	}
	
	return args;
}

static const ELFSymbol_t symtab[] = {
	{ "ST7735_Message", ST7735_Message }
};

void Help_Command(char* args[]){
	
	//RPC decoding
	args = RPC_Decode(args);
	
	UART_OutString("---Commands:---\r\n");
	UART_OutString("help\r\n");
	UART_OutString("lcd_top clear\r\n");
	UART_OutString("lcd_top <message (no spaces)> <value> <line>\r\n");
	UART_OutString("lcd_bot clear\r\n");
	UART_OutString("lcd_bot <message (no spaces)> <value> <line>\r\n");
	UART_OutString("os_ms show\r\n");
	UART_OutString("os_ms clear\r\n");
	UART_OutString("adc sample\r\n");
	UART_OutString("measurements show\r\n");
	UART_OutString("filesys start\r\n");
	UART_OutString("filesys format\r\n");
	UART_OutString("filesys pdir\r\n");
	UART_OutString("filesys create <filename>\r\n");
	UART_OutString("filesys write <filename> <message>\r\n");
	UART_OutString("filesys pfile <filename>\r\n");
	UART_OutString("filesys delete <filename>\r\n");
	UART_OutString("filesys close\r\n");
	
	UART_OutString("load user \r\n");
	UART_OutString("Wifi server \r\n");
	UART_OutString("Wifi client\r\n");
	UART_OutString("Wifi disconnect\r\n");
	UART_OutString("led red\r\n");
	UART_OutString("led blue\r\n");
	UART_OutString("led green\r\n");
	
	UART_OutString("client <one of previous commands>\r\n");

}

//should change these methods to prevent long if else chains/switch case
void Lcd_Top_Command(char* args[]){
	
	//RPC decoding
	args = RPC_Decode(args);
	
	if(strcmp(args[1], "clear") == 0){
		ST7735_FillRect(0, 0, ST7735_TFTWIDTH, ST7735_TFTHEIGHT/2, ST7735_BLACK);
	}else	if(args[1] != NULL && args[2] != NULL && args[3] != NULL){						//!!!! clean up command syntax!!!!!!!	
		ST7735_Message(0, atoi(args[3]), args[1], atoi(args[2]));
	}
}

void Lcd_Bot_Command(char* args[]){
	
	//RPC decoding
	args = RPC_Decode(args);
	
	if(strcmp(args[1], "clear") == 0){
		ST7735_FillRect(0, ST7735_TFTHEIGHT/2, ST7735_TFTWIDTH, ST7735_TFTHEIGHT/2, ST7735_BLACK);
	}else	if(args[1] != NULL && args[2] != NULL && args[3] != NULL){
		ST7735_Message(1, atoi(args[3]), args[1], atoi(args[2]));
	}
}

void Os_Ms_Command(char* args[]){
	
	//RPC decoding
	args = RPC_Decode(args);
	
	if(strcmp(args[1], "show") == 0){
		char buffer[20];
		sprintf(buffer, "%d", OS_MsTime());	//ST7735_Message only outputs int32_t not uint32_t so convert to string
		Interpreter_OutString("MS Time: ");
		Interpreter_OutString(buffer);
	}else if(strcmp(args[1], "clear") == 0){
		OS_ClearMsTime();
	}
}

void Adc_Command(char* args[]){
	
	//RPC decoding
	//client 4 se6 
	args = RPC_Decode(args);
	//args = adc sample
	
	//responds to client and tm4c adc requests
	if(strcmp(args[1], "sample") == 0){ //client 
		Interpreter_OutString("ADC: "); //extra part only for client responses
		char buffer[20];
		sprintf(buffer, "%d", ADC_In());	
		Interpreter_OutString(buffer);
	}
	
	
}
extern uint32_t NumCreated;
//extern uint32_t PIDWork;
//extern uint32_t DataLost;
extern uint32_t MaxJitter;
void Measurements_Command(char* args[]){
	
	//RPC decoding
	//args = RPC_Decode(args);
	
	if(strcmp(args[1], "show") == 0){
		ST7735_Message(0,0,"NumCreated =",NumCreated);
		//ST7735_Message(0,1,"PIDWork     =",PIDWork);
		//ST7735_Message(0,2,"DataLost    =",DataLost);
		ST7735_Message(0,3,"Jitter 0.1us=",MaxJitter);
	}
}

void Load_Command(char* args[]){
	
	//RPC decoding
	args = RPC_Decode(args);
	
	if(strcmp(args[1], "user") == 0){
		ELFEnv_t env = {symtab, 1};
		
		if(exec_elf("User.axf", &env) == 1){
			Interpreter_OutString("Load successful");
		}
		else{
			Interpreter_OutString("Load unsuccessful");
		}
	}//else -----load other filename
}

char ClientResponse[64];
#define RETURN_COMMANDS_NUM 2
const char* ReturnCommands[RETURN_COMMANDS_NUM] = {
	"adc",
	"os_ms"
};
//assume the connection has been made, "client arg1 arg2" calls this function 
//This function calls RPC_Encoding stuff to send to ESp
void Client_Command(char* args[]){
	char input[40];
	int index = 0;
	
	//encoding RPC
	args = RPC_Encode(args);

	//ESP_Send needs a string, not array of strings
	for(int i = 0; i < MAX_ARGS; i++){	
		for(int j = 0; j < strlen(args[i]); j++){
			input[index] = args[i][j];
			index++;
		}
		input[index] = ' ';	//add the space
		index++;
	}
	
	
	const char* fetch = strcat(input, "\n\r");	// example client 10 rd1, fetch = led blue
	ESP8266_Send(fetch); //client sends to server, inside fetch it receives char* fetch from sever 
	bool receive = false;
	
	//checking if client asked os_ms or adc is called
	for(int i = 0; i < RETURN_COMMANDS_NUM; i++){
		if(strncmp(ReturnCommands[i], fetch, strlen(ReturnCommands[i])) ==0){
			receive = true;
		}
	}
	//os_ms or adc is called, waits for a response from server
	if(receive){
		ESP8266_Receive(ClientResponse, 64);
		UART_OutString(ClientResponse);
		UART_OutString("\r\n");
	}
	
}

void Wifi_Command(char* args[]){
	
	 if (strcmp(args[1], "server") == 0){
		UART_OutString("starting...");
		OS_AddThread(&Wifi_Server,128,2);
	}
	else if (strcmp(args[1], "client") == 0){
		UART_OutString("starting...");
		OS_AddThread(&Wifi_Client,128,2);
		
	}else if (strcmp(args[1], "disconnect") == 0){
		UART_OutString("starting...");
		OS_AddThread(&Disconnect_Server,128,2);

	}
}

void LED_Command(char* args[]){
	
	//RPC decoding
	args = RPC_Decode(args);
	
	if(strcmp(args[1], "red") == 0){
		PF1 ^= 0x02;
	}else if(strcmp(args[1], "green") == 0){
		PF3 ^= 0x08;
	}else if(strcmp(args[1], "blue") == 0){
		PF2 ^= 0x04;
	}
}

char const string1[]="Filename = %s";
char const string2[]="File size = %lu bytes";
char const string3[]="Number of Files = %u";
char const string4[]="Number of Bytes = %lu";
void TestDirectory(void){ char *name; unsigned long size; 	
	  unsigned int num;	
	  unsigned long total;	
	  num = 0;	
	  total = 0;	
	  printf("\n\r");	
	  if(eFile_DOpen(""))           UART_OutString("eFile_DOpen");	
	  while(!eFile_DirNext(&name, &size)){	
	    printf(string1, name);	
	    printf("  ");	
	    printf(string2, size);	
	    printf("\n\r");	
	    total = total+size;	
	    num++;    	
	  }	
	  printf(string3, num);	
	  printf("\n\r");	
	  printf(string4, total);	
	  printf("\n\r");	
	  if(eFile_DClose())            UART_OutString("eFile_DClose");	
	} 
		
//change this to a list instead of if elses?	
void Filesys_Command(char* args[]){	
	UART_OutString("\n\r");		//for clean putty output
	
	//RPC decoding
	args = RPC_Decode(args);
	
	if(strcmp(args[1], "start") == 0){	
		eFile_Init();	
		eFile_Mount();	//given eFile requires mount to be called
	}else if(strcmp(args[1], "format") == 0){	
		eFile_Init();
		eFile_Mount();		
		eFile_Format();	
	}else if(strcmp(args[1], "pdir") == 0){	
		TestDirectory();	
	}else if(strcmp(args[1], "create") == 0){	
		if(eFile_Create(args[2])){		//name of file	
			Interpreter_OutString("Failed to create file");	
		}else{	
			Interpreter_OutString("Created file");	
		}	
	}else if(strcmp(args[1], "write") == 0){	
		if(eFile_WOpen(args[2])){	
			Interpreter_OutString("Failed to open file");	
		}else{	
			int i = 0;	
			while(args[3][i] != NULL){	
				if(eFile_Write(args[3][i])){	
					Interpreter_OutString("Failed to write character");	
				}	
				i++;	
			}	
			eFile_WClose();	
		}	
	}	
	else if(strcmp(args[1], "pfile") == 0){	
		if(eFile_ROpen(args[2]))      Interpreter_OutString("eFile_ROpen fail");	
			
		char data;	
		char fullString[512];
		uint32_t index = 0;
		while(!eFile_ReadNext(&data)){	
			fullString[index] = data;
			index++;
		}	
		Interpreter_OutString(fullString);
		
		for(uint32_t i = 0; i < 512; i++){
			fullString[i] = NULL;	//flush
		}
		
		eFile_RClose();	
	}else if(strcmp(args[1], "delete") == 0){	
		if(eFile_Delete(args[2]))     Interpreter_OutString("could not delete");	
	}else if(strcmp(args[1], "close") == 0){	
		eFile_Unmount();
	}
}
