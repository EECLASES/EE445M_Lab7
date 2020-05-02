/**
 * @file      Command.h
 * @brief     For Real Time Operating System Labs
 * @details   EE445M
 * @version   V1.0
 * @author    Phat & Miguel
 * @warning   AS-IS
 * @note      Where we set commands that get executed if the argument matches the trigger
 * @date      Feb 3, 2020

 ******************************************************************************/
 
#include "../inc/CommandExecute.h"	//holds the function definitions
#include <stdint.h>
//print adc value, print ostimer, clear ostimer
#define NUMCOMMANDS 8


typedef void(*functionPtr)(char* args[]);

typedef struct {
	char* trigger;
	functionPtr command;
}PartCommands;		


//array for struct of commands
PartCommands Commands[NUMCOMMANDS] = {		
	{"help", &Help_Command},
	{"lcd_top", &Lcd_Top_Command},
	{"lcd_bot", &Lcd_Bot_Command},
	{"os_ms", &Os_Ms_Command},
	{"adc", &Adc_Command},
	{"measurements", &Measurements_Command},
	{"filesys", &Filesys_Command},
	{"load", &Load_Command}
};

