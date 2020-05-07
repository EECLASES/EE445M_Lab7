/**
 * @file      Interpreter.h
 * @brief     Real Time Operating System for Labs 2, 3 and 4
 * @details   EE445M/EE380L.6
 * @version   V1.0
 * @author    Valvano
 * @copyright Copyright 2020 by Jonathan W. Valvano, valvano@mail.utexas.edu,
 * @warning   AS-IS
 * @note      For more information see  http://users.ece.utexas.edu/~valvano/
 * @date      Jan 5, 2020

 ******************************************************************************/

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

typedef struct WifiStatus {
	int8_t initEnable;
	int8_t mode;
	int MAC;
	bool connected;
} WifiStatus;



/**
 * @details  Run the user interface.
 * @param  none
 * @return none
 * @brief  Interpreter
 */
void Interpreter(void);

void Wifi_Switch(bool mode);
void Wifi_Server(void);
void Wifi_Client(void);
void Disconnect_Server(void);

//Outstring code from labdoc
void Interpreter_OutString(char *s);

//Instring interpreter wrapper
void Interpreter_InString(void);
