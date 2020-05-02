/**
 * @file      CommandExecute.h
 * @brief     For Real Time Operating System Labs
 * @details   EE445M
 * @version   V1.0
 * @author    Phat & Miguel
 * @warning   AS-IS
 * @note      Function definitions of the command functions that can be executed
 * @date      Feb 3, 2020

 ******************************************************************************/

void Help_Command(char* args[]);

void Lcd_Top_Command(char* args[]);	//maybe split these to different files

void Lcd_Bot_Command(char* args[]);

void Os_Ms_Command(char* args[]);

void Adc_Command(char* args[]);

void Measurements_Command(char* args[]);

void Filesys_Command(char* args[]);

void Load_Command(char* args[]);
