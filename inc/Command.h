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
#define NUMCOMMANDS 11
#define RPC_NUM 23
#define MAX_ARGS 10

typedef void(*functionPtr)(char* args[]);

typedef struct {
	char* trigger;
	functionPtr command;
}PartCommands;		

typedef struct {
	char* human;
	char* protocol;
}RPC_Convert;

extern RPC_Convert RPC_Protocol[RPC_NUM];

//array for struct of commands
extern PartCommands Commands[NUMCOMMANDS];

