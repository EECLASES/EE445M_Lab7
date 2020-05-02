/**
 * @file      PCB.h
 * @brief     Real Time Operating System for Labs 1, 2, 3 and 4
 * @details   EE445M/EE380L.6
 * @version   V1.0
 * @author    Valvano
 * @copyright Copyright 2020 by Jonathan W. Valvano, valvano@mail.utexas.edu,
 * @warning   AS-IS
 * @note      For more information see  http://users.ece.utexas.edu/~valvano/
 * @date      Jan 12, 2020

 ******************************************************************************/



 
#ifndef __PCB_H
#define __PCB_H  1
#include <stdint.h>
#include <stdio.h>
#include "../RTOS_Labs_common/OS.h"

typedef struct tid_list_item {
    int val;
		void* text;
		void* data;
    struct tid_list_item* nextChild;
} tid_list_item;


typedef struct PCB{
	int32_t pid ;
	int32_t size;
	struct tid_list_item* headChildTid;
	
	
} PCB;


#endif
