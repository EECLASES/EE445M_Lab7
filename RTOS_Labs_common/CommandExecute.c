
#include <stdio.h>
#include <stdbool.h> 
#include <stdlib.h>
#include <string.h>

#include "../RTOS_Labs_common/ST7735.h"
#include "../RTOS_Labs_common/OS.h"
#include "../RTOS_Labs_common/ADC.h"
#include "../RTOS_Labs_common/UART0int.h"
#include "../RTOS_Labs_common/eFile.h"
#include "../inc/CommandExecute.h"
#include "../RTOS_Labs_common/esp8266.h"
#include "../inc/loader.h"
#include "../RTOS_Labs_common/Interpreter.h"

extern int ServerID;

static const ELFSymbol_t symtab[] = {
	{ "ST7735_Message", ST7735_Message }
};

void Help_Command(char* args[]){
	//UART_OutString("help");
	ST7735_Message(0, 0, "Help", -10);	//output to uart instead?
	ST7735_Message(0, 1, "lcd_top", 1);
	ST7735_Message(0, 2, "lcd_bot", 2);
	ST7735_Message(0, 3, "os_ms", 3);
	ST7735_Message(0, 4, "adc", 4);
	ST7735_Message(0, 5, "measurements", 5);
	ST7735_Message(0, 6, "load", 6);
	ST7735_Message(0, 6, "wifi", 6);

}

//should change these methods to prevent long if else chains/switch case
void Lcd_Top_Command(char* args[]){
	if(strcmp(args[1], "clear") == 0){
		ST7735_FillRect(0, 0, ST7735_TFTWIDTH, ST7735_TFTHEIGHT/2, ST7735_BLACK);
	}else	if(args[1] != NULL && args[2] != NULL && args[3] != NULL){						//!!!! clean up command syntax!!!!!!!	
		ST7735_Message(0, atoi(args[3]), args[1], atoi(args[2]));
	}
}

void Lcd_Bot_Command(char* args[]){
	if(strcmp(args[1], "clear") == 0){
		ST7735_FillRect(0, ST7735_TFTHEIGHT/2, ST7735_TFTWIDTH, ST7735_TFTHEIGHT/2, ST7735_BLACK);
	}else	if(args[1] != NULL && args[2] != NULL && args[3] != NULL){
		ST7735_Message(1, atoi(args[3]), args[1], atoi(args[2]));
	}
}

void Jitter_Command(char* args[]){
	if(strcmp(args[1], "print") == 0){
		Jitter();
	}
}

void Os_Ms_Command(char* args[]){
	if(strcmp(args[1], "show") == 0){
		char buffer[20];
		sprintf(buffer, "%d", OS_MsTime());	//ST7735_Message only outputs int32_t not uint32_t so convert to string
		ST7735_Message(0, 0, buffer, -1);
	}else if(strcmp(args[1], "clear") == 0){
		OS_ClearMsTime();
	}
}

void Adc_Command(char* args[]){
	if(strcmp(args[1], "sample") == 0){
		ST7735_Message(0, 0, "ADC:", ADC_In());
	}
}
extern uint32_t NumCreated;
//extern uint32_t PIDWork;
//extern uint32_t DataLost;
extern uint32_t MaxJitter;
void Measurements_Command(char* args[]){
	if(strcmp(args[1], "show") == 0){
		ST7735_Message(0,0,"NumCreated =",NumCreated);
		//ST7735_Message(0,1,"PIDWork     =",PIDWork);
		//ST7735_Message(0,2,"DataLost    =",DataLost);
		ST7735_Message(0,3,"Jitter 0.1us=",MaxJitter);
	}
}

void Load_Command(char* args[]){
	
	if(strcmp(args[0], "w") == 0){
		if(strcmp(args[1], "u") == 0){
			args[1] = "user";
		}
	}
	
	if(strcmp(args[1], "user") == 0){
		ELFEnv_t env = {symtab, 1};
		
		if(exec_elf("User.axf", &env) == 1){
			Interpreter_OutString("Load successful");
		}
		else{
			Interpreter_OutString("Load unsuccessful");
		}
	}
}
void Wifi_Command(char* args[]){
	
	if(strcmp(args[1], "on") == 0){
		UART_OutString("starting.. ");
		Wifi_Switch(1);
	}
	else if (strcmp(args[1], "off") == 0){
		UART_OutString("starting...");

		Wifi_Switch(0);
	}
	else if (strcmp(args[1], "server") == 0){
		UART_OutString("starting...");
		OS_AddThread(&Wifi_Server,128,2);
	}
	else if (strcmp(args[1], "client") == 0){
		UART_OutString("starting...");
		Wifi_Client(0);
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
	if(strcmp(args[0], "w") == 0){
		if(strcmp(args[1], "s") == 0){
			args[1] = "start";
		}else if(strcmp(args[1], "f") == 0){
			args[1] = "format";
		}else if(strcmp(args[1], "pd") == 0){
			args[1] = "pdir";
		}else if(strcmp(args[1], "c") == 0){
			args[1] = "create";
		}else if(strcmp(args[1], "w") == 0){
			args[1] = "write";
		}else if(strcmp(args[1], "pf") == 0){
			args[1] = "pfile";
		}else if(strcmp(args[1], "d") == 0){
			args[1] = "delete";
		}else if(strcmp(args[1], "c") == 0){
			args[1] = "close";
		}
	}
	
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
