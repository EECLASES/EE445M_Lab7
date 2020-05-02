// filename *************************heap.c ************************
// Implements memory heap for dynamic memory allocation.
// Follows standard malloc/calloc/realloc/free interface
// for allocating/unallocating memory.

// Jacob Egner 2008-07-31
// modified 8/31/08 Jonathan Valvano for style
// modified 12/16/11 Jonathan Valvano for 32-bit machine
// modified August 10, 2014 for C99 syntax

/* This example accompanies the book
   "Embedded Systems: Real Time Operating Systems for ARM Cortex M Microcontrollers",
   ISBN: 978-1466468863, Jonathan Valvano, copyright (c) 2015

 Copyright 2015 by Jonathan W. Valvano, valvano@mail.utexas.edu
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



/*TODO:
	finish free
	finish realloc
	corrupt heap check?
*/

#include <stdint.h>
#include <stdbool.h>
#include "../RTOS_Labs_common/heap.h"

#define NULL 0

static int32_t Heap[HEAP_ROWS];

//tells if the pointer is still inside the heap
bool inHeap(int32_t *ptr){
	return ptr >= &Heap[0] && ptr <= &Heap[HEAP_ROWS - 1];
}

//marks free space in heap as used, split if applicable
void markUsed(int32_t *ptr, int32_t desiredRows){
	int32_t blockRows = -1 * (*ptr);
	
	if(blockRows == desiredRows){	//exact
		*ptr = desiredRows;
		*(ptr + blockRows + 1) = desiredRows;
		
	}else{	//split
		*ptr = desiredRows;
		
		int32_t *tail = ptr + desiredRows + 1;	//marker for end of block
		*tail = desiredRows;
		
		*(tail + 1) = -1*(blockRows - desiredRows - 2);	//update the split block, two less space b/c of markers
		*(ptr + blockRows + 1) = -1*(blockRows - desiredRows - 2);
		
	}
}

//checks to see if pointer is at beginning of block
bool validPointer(int32_t *ptr){
	int32_t *itr = &Heap[0];	//this goes through the head markers!
	
	while(inHeap(itr)){
		if((itr + 1) == ptr){		//ptr is the actual pointer the user uses (one element from marker)
			return true;
		}
		
		int32_t jump = *itr;
		if(jump < 0){
			jump *= -1;
		}
		itr += jump + 2;
		
	}
	
	return false;
}

//given a block marker, merges free block with free block below
void mergeWithBot(int32_t *ptr){
	int32_t topSize = *ptr;	//negative
	
	int32_t *bottomPtr = ptr + (-1)*topSize + 2;	//this gets removed
	int32_t bottomSize = *bottomPtr;	//negative
	int32_t *tail = bottomPtr + (-1)*bottomSize + 1;	//this gets updated
	
	int32_t newSize = topSize + bottomSize - 2;
	
	*ptr = newSize;
	*tail = newSize;
	
}

//******** Heap_Init *************** 
// Initialize the Heap
// input: none
// output: always 0
// notes: Initializes/resets the heap to a clean state where no memory
//  is allocated.
int32_t Heap_Init(void){
	
	Heap[0] = -1*(HEAP_ROWS - 2);	//first and last elements tell how many words are free
	Heap[HEAP_ROWS - 1] = -1*(HEAP_ROWS - 2);
	
  return SUCCESS;   
}


//******** Heap_Malloc *************** 
// Allocate memory, data not initialized
// input: 
//   desiredBytes: desired number of bytes to allocate
// output: void* pointing to the allocated memory or will return NULL
//   if there isn't sufficient space to satisfy allocation request
void* Heap_Malloc(int32_t desiredBytes){
 
	if(desiredBytes < 0){
		return NULL; //byte check first, worry about fragmentation later in function
	}
	
	int32_t desiredRows = (desiredBytes + sizeof(int32_t) - 1)/sizeof(int32_t);	//integer division truncates
	int32_t *pointer = &Heap[0];	//!this goes through the markers
	
	//first fit
	while(inHeap(pointer)){
		int32_t blockSize = *pointer;
		
		if(blockSize < 0 && desiredRows <= (-1)*blockSize){
			markUsed(pointer, desiredRows);
			return (pointer + 1);	//pointer is going through size markers so actual heap is one after
		}
		
		if(blockSize > 0){
			pointer += blockSize + 2; //+1 is tail marker
		}else{
			pointer += (-1)*blockSize + 2;
		}
		
	}
	
  return NULL;   // NULL
}


//******** Heap_Calloc *************** 
// Allocate memory, data are initialized to 0
// input:
//   desiredBytes: desired number of bytes to allocate
// output: void* pointing to the allocated memory block or will return NULL
//   if there isn't sufficient space to satisfy allocation request
//notes: the allocated memory block will be zeroed out
void* Heap_Calloc(int32_t desiredBytes){  
	
	int32_t *ptr = Heap_Malloc(desiredBytes);
	
	if(ptr == NULL) return NULL;
	
	int32_t desiredRows = *(ptr - 1);	//number of rows stored in marker
	
	for(uint32_t i = 0; i < desiredRows; i++){
		ptr[i] = 0;
	}
	
  return ptr;  
}


//******** Heap_Realloc *************** 
// Reallocate buffer to a new size
//input: 
//  oldBlock: pointer to a block
//  desiredBytes: a desired number of bytes for a new block
// output: void* pointing to the new block or will return NULL
//   if there is any reason the reallocation can't be completed
// notes: the given block may be unallocated and its contents
//   are copied to a new block if growing/shrinking not possible
void* Heap_Realloc(void* oldBlock, int32_t desiredBytes){
 
	int32_t *oldBlockPtr = (int32_t *)oldBlock;
	
	if(!inHeap(oldBlockPtr)) return NULL;
	
	if(*(oldBlockPtr - 1) < 0) return NULL;	//pointer is to an empty/free block
	
	if(!validPointer(oldBlockPtr)) return NULL;	//make sure pointer is at beginning of a block
	
	int32_t *newBlockPtr = Heap_Malloc(desiredBytes);
	
	if(newBlockPtr == NULL) return NULL;
	
	int32_t oldBlockSize = *(oldBlockPtr - 1);
	int32_t newBlockSize = *(newBlockPtr - 1);
	
	int32_t smallerSize;
	if(oldBlockSize < newBlockSize){
		smallerSize = oldBlockSize;
	}else{
		smallerSize = newBlockSize;
	}
	
	for(uint32_t i = 0; i < smallerSize; i++){
		newBlockPtr[i] = oldBlockPtr[i];
	}
	
	if(Heap_Free(oldBlockPtr) != SUCCESS) return NULL;
	
  return newBlockPtr;   
}


//******** Heap_Free *************** 
// return a block to the heap
// input: pointer to memory to unallocate
// output: 0 if everything is ok, non-zero in case of error (e.g. invalid pointer
//     or trying to unallocate memory that has already been unallocated
int32_t Heap_Free(void* pointer){
 
	int32_t *blockPtr = (int32_t *)pointer;
	
	if(!inHeap(blockPtr)) return FAIL;
	
	if(*(blockPtr - 1) < 0) return FAIL;	//pointer is to an empty/free block
	
	if(!validPointer(blockPtr)) return FAIL;	//make sure pointer is at beginning of a block
	
	int32_t *blockEndPtr = blockPtr + *(blockPtr - 1);
	if(!inHeap(blockEndPtr) || *(blockEndPtr) < 0) return FAIL;	//corruption check
	
	int32_t blockSize = *(blockPtr - 1);
	
	//free block first before checking neighbors
	if(*(blockPtr - 1) != *(blockPtr + blockSize)) return FAIL;	//corruption
	
	*(blockPtr - 1) *= -1;
	*(blockPtr + blockSize) *= -1;
	
	//check neighbors
	int32_t *topCheck = blockPtr - 2;
	if(inHeap(topCheck) && *topCheck < 0){	//if block above exists and is also free
		topCheck += *(topCheck);	//free block markers are negative already
		topCheck -= 1;
		
		mergeWithBot(topCheck);
		blockPtr = topCheck + 1;		//hold top block pointer now to check below
	}
	
	int32_t *bottomCheck = blockPtr - *(blockPtr - 1) + 1;
	if(inHeap(bottomCheck) && *bottomCheck < 0){	//if block below exists and is also free
		mergeWithBot(blockPtr - 1);
	}
	
  return SUCCESS;   // replace
}


//******** Heap_Stats *************** 
// return the current status of the heap
// input: reference to a heap_stats_t that returns the current usage of the heap
// output: 0 in case of success, non-zeror in case of error (e.g. corrupted heap)
int32_t Heap_Stats(heap_stats_t *stats){
	//just iterrate through
	int32_t *blockHead = &Heap[0];
	
	stats->free = 0;
	stats->size = 0;
	stats->used = 0;
	
	while(inHeap(blockHead)){
		if(*blockHead > 0){
			stats->size += *blockHead * sizeof(int32_t);
			stats->used += *blockHead * sizeof(int32_t);
			
			blockHead += *blockHead + 2;
		}else{
			stats->size += (-1)* (*blockHead) * sizeof(int32_t);
			stats->free += (-1)* (*blockHead) * sizeof(int32_t);
			
			blockHead += (-1)*(*blockHead) + 2;
		}
		
	}
	
  return SUCCESS;   // replace
}
