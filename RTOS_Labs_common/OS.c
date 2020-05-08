// *************os.c**************
// EE445M/EE380L.6 Labs 1, 2, 3, and 4 
// High-level OS functions
// Students will implement these functions as part of Lab
// Runs on LM4F120/TM4C123
// Jonathan W. Valvano 
// Jan 12, 2020, valvano@mail.utexas.edu


#include <stdint.h>
#include <stdio.h>
#include "../inc/tm4c123gh6pm.h"
#include "../inc/CortexM.h"
#include "../inc/PLL.h"
#include "../inc/LaunchPad.h"
#include "../inc/Timer4A.h"
#include "../inc/WTimer0A.h"
#include "../RTOS_Labs_common/OS.h"
#include "../RTOS_Labs_common/PCB.h"
#include "../RTOS_Labs_common/ST7735.h"
#include "../inc/ADCT0ATrigger.h"
#include "../RTOS_Labs_common/UART0int.h"
#include "../RTOS_Labs_common/eFile.h"
#include "../RTOS_Labs_common/eDisk.h"
#include "../RTOS_Labs_common/ADC.h"
#include "../RTOS_Labs_common/Heap.h"


#include "../inc/Timer5A.h"
#include "../inc/WTimer1A.h"
#include "../inc/WTimer1B.h"
extern uint32_t NumCreated;

// Performance Measurements 
int32_t MaxJitter;             // largest time jitter between interrupts in usec
#define JITTERSIZE 64
uint32_t const JitterSize=JITTERSIZE;
uint32_t JitterHistogram1[JITTERSIZE]={0,};

int32_t MaxJitter2;             // largest time jitter between interrupts in usec
#define JITTERSIZE 64
uint32_t const JitterSize2=JITTERSIZE;
uint32_t JitterHistogram2[JITTERSIZE]={0,};

uint32_t Period1;
uint32_t Period2;

#include <stdlib.h>

//moved TCB struct to header
TCB* ActiveHead;		//head of linked list
TCB* ActiveFoot;		//tracks footer of linked list
TCB* RunPt;				//runs through linked list

TCB* SleepHead;		//head of linked list, sleep list uses the TCB's prev pointers

extern void ContextSwitch(void);
extern void StartOS(void);

static uint32_t MsTime;
static uint32_t UntouchedTime;
static uint32_t systemTime;

void(incrementTime)(void);


//mailbox variables
Sema4Type BoxFree;
Sema4Type DataValid;
uint32_t	Mailbox;

//adds a given node to the end of the active list
static void addToActive(TCB* node){
	uint32_t sr = StartCritical();
	
	if(ActiveHead == NULL){			
		ActiveHead = node;						//first thread to be in list
		ActiveHead->next = ActiveHead;	//set next and previous to self because only one
		ActiveHead->prev = ActiveHead;	
		ActiveFoot = ActiveHead;				//set foot tracker
	}else{
		/*//before priority
		ActiveFoot->next = node;
		node->prev = ActiveFoot;
		node->next = ActiveHead;
		
		ActiveHead->prev = node;
		ActiveFoot = ActiveFoot->next;  */
		
		//priority
		TCB* itr = ActiveHead;
		uint8_t placed = 0;
		
		while(itr != ActiveFoot){
			if(node->priority < itr->priority){	//'higher' priority
				node->prev = itr->prev;
				node->next = itr;
				
				itr->prev->next = node;
				itr->prev = node;
				
				if(itr == ActiveHead){
					ActiveHead = node;		//if placing at the 'front' of the list then node is highest priority so change head
				}
				
				placed = 1;
				break;
			}
			itr = itr->next;
		}
		
		if(placed == 0){	//node being added is lowest priority in list
			if(node->priority < itr->priority){	//'higher' priority
				node->prev = itr->prev;
				node->next = itr;
				
				itr->prev->next = node;
				itr->prev = node;
				
				if(itr == ActiveHead){
					ActiveHead = node;		//if placing at the 'front' of the list then node is highest priority so change head
				}
				
				placed = 1;
			}else{
				node->prev = ActiveFoot;
				node->next = ActiveFoot->next;
				
				ActiveFoot->next->prev = node;
				ActiveFoot->next = node;
				
				ActiveFoot = node;
			}
		} 
		
	}		
	
	EndCritical(sr);
}


void Scheduler(void){
	RunPt = ActiveHead;		//assumes always an idle thread
	//RunPt = RunPt->next;	//round robin
}

//Unlinks a given node from active list, the node's next and prev are untouched
//returns a pointer to the node
static TCB* unlinkNodeFromActive(TCB* node){

	uint32_t sr = StartCritical();
	
	node->prev->next = node->next;
	node->next->prev = node->prev;
	
		//change end pointers if applicable
	if(ActiveFoot == node){
		ActiveFoot = node->prev;
	}
	if(ActiveHead == node){
		ActiveHead = node->next;
	}
	
	
	EndCritical(sr);
	
	return node;
	
}


//what happens when going through the sleep list
static void sleepTask(void){
	//PF2 ^= 0x04;
	
	TCB* iterator = SleepHead;
	TCB* follower = NULL;
	
	//uint32_t sleepSlice = (SLEEPPERIOD * 12.5 / 100000);
	
	while(iterator != NULL){
		
		//kill check
		if(iterator->sleep == KILLSTAT){
			TCB* toKill = iterator;
			
			//delinking thread to kill
			if(SleepHead == toKill){
				SleepHead = toKill->prev;
			}
			
			iterator = iterator->prev;
				
			if(follower != NULL){
				follower->prev = iterator;	
			}
			
			if(toKill->pcb != NULL){	
				if(toKill->pcb->size == 0){	
					free(toKill->pcb);	
				}	
			}
			
			free(toKill);
			
		}	else if(iterator != NULL){ //sleep decrement
			
			if(iterator->sleep < UntouchedTime){ //thread has slept enough time
				//wake up thread
				iterator->sleep = 0; //????
				
				TCB* waking = iterator;
				
				iterator = waking->prev;	//move iterator before waking up
				
				if(follower != NULL){		//don't access null if first node is waking up
					follower->prev = iterator; //fix linking of previous node
				}else{
					SleepHead = iterator;	//first node is waking up so change SleepHead
				}
				
				addToActive(waking);		//wake up thread
				
			}else{
				//just keep going through list
				follower = iterator;
				iterator = iterator->prev;
			}
			
		}
		
	} 
}

//initializes wide timer 2 for measuring time
void OS_InitTimeMeasure(){
	SYSCTL_RCGCWTIMER_R |= 0x04;   // 0) activate WTIMER2
  WTIMER2_CTL_R = 0x00000000;    // 1) disable WTIMER2A during setup
  WTIMER2_CFG_R = 0x00000000;    // 2) configure for 64-bit mode
  WTIMER2_TAMR_R = 0x00000011;   // 3) configure for one-shot up-count (pg 729 datasheet)
  WTIMER2_TAILR_R = 0;    // 4) reload value
	WTIMER2_TBILR_R = 0;           // bits 63:32
  WTIMER2_TAPR_R = 0;            // 5) bus clock resolution
  WTIMER2_ICR_R = 0x00000001;    // 6) clear WTIMER2A timeout flag 
}

void OS_TimeMeasureStart(){
	if(!WTIMER2_CTL_R){		//incase of nested
		WTIMER2_TAILR_R = 0;
		WTIMER2_CTL_R = 0x00000001;    // 10) enable TIMER2A
	}
}

uint32_t GreatestTime = 0;	//finds largest time (between interrupts)
void OS_TimeMeasureEnd(){
	if(WTIMER2_CTL_R){
		WTIMER2_CTL_R = 0x00000000;    // 1) disable WTIMER2A
		uint32_t measured = WTIMER2_TAILR_R;
		if(measured > GreatestTime){
			GreatestTime = measured;
		}
	}
}

/*------------------------------------------------------------------------------
  Systick Interrupt Handler
  SysTick interrupt happens every 10 ms
  used for preemptive thread switch
 *------------------------------------------------------------------------------*/
void SysTick_Handler(void) {
	long sr = StartCritical();
	if(RunPt->overflowcheck != 0 || RunPt->overflowcheckbot != 0){
		UART_OutString("stack overflow!");
		OS_Kill();	//find real way to deal with overflow
	}
  //PF1 ^= 0x02;	//profiling systick
	//PF1 ^= 0x02;
  ContextSwitch();
	//PF1 ^= 0x02;
	EndCritical(sr);
} // end SysTick_Handler

unsigned long OS_LockScheduler(void){
  // lab 4 might need this for disk formating
  return 0;// replace with solution
}
void OS_UnLockScheduler(unsigned long previous){
  // lab 4 might need this for disk formating
}


void SysTick_Init(unsigned long period){
  long sr = StartCritical();
  NVIC_ST_CTRL_R = 0;         // disable SysTick during setup
  NVIC_ST_RELOAD_R = period-1;// reload value
  NVIC_ST_CURRENT_R = 0;      // any write to current clears it
	NVIC_ST_CTRL_R = 0x07;	// enable SysTick with core clock and interrupts
  EndCritical(sr);
}

/**
 * @details  Initialize operating system, disable interrupts until OS_Launch.
 * Initialize OS controlled I/O: serial, ADC, systick, LaunchPad I/O and timers.
 * Interrupts not yet enabled.
 * @param  none
 * @return none
 * @brief  Initialize OS
 */
void OS_Init(void){
  // put Lab 2 (and beyond) solution here
	DisableInterrupts();
	
	PLL_Init(Bus80MHz);
  UART_Init();       // serial I/O for interpreter
	Heap_Init();
	eFile_Init();
	eFile_Mount();
	ST7735_InitR(INITR_REDTAB); // LCD initialization
	//ST7735_InitB()
  LaunchPad_Init();  // debugging profile on PF1
  //ADC_Init(3);       // channel 3 is PE0 <- connect an IR distance sensor to J5 to get a realistic analog signal   
	//Timer4A_Init(&DAStask,80000000/10,1); // 10 Hz sampling, priority=1
	SysTick_Init(1);	//
	OS_ClearMsTime();    // start a periodic interrupt to maintain time
	Timer5A_Init(&sleepTask, SLEEPPERIOD, 1);	//100ms ---------------------------NOTE: change for os improvement
	
	WideTimer0A_Init(&incrementTime, 80000000/1000, 0); //OS time timer
	
	SleepHead = NULL;
	ActiveHead = NULL;
	ActiveFoot = NULL;
	
	UntouchedTime = 0;
	systemTime = 0;

}; 

/*	// ******** OS_Init_FileLock************ MIGUEL	
	// initialize lock and semaphore within	
	// input:  pointer to a FileLock 	
	// output: none	
	void OS_Init_FileLock(struct FileLock* fLock ){	
		
			
		fLock->file = NULL;	
		OS_InitSemaphore( fLock->fileSemaphore, 1 );	
		
	}	
		
	// ******** release_FileLock************	
	// acquire lock and semaphore within	
	// input:  pointer to a FileLock 	
	// output: none	
	void acquireFileLock(struct FileLock* fLock, struct File* file ){	
			
				
		OS_Wait(fLock->fileSemaphore);	
			
		fLock->file = file;	
		
		
	}	
		
		
	// ******** release_FileLock************	
	// releases lock and semaphore within	
	// input:  pointer to a FileLock 	
	// output: none	
	void releaseFileLock(struct FileLock* fLock ){	
		
		fLock->file = NULL;	
		OS_Signal(fLock->fileSemaphore);	
	} */

// ******** OS_InitSemaphore ************
// initialize semaphore 
// input:  pointer to a semaphore
// output: none
void OS_InitSemaphore(Sema4Type *semaPt, int32_t value){
  // put Lab 2 (and beyond) solution here
	semaPt->Value = value;
	
	//for blocking lab3
	semaPt->head = NULL;
	//semaPt->foot = NULL;
}; 


// ******** OS_Wait ************
// decrement semaphore 
// Lab2 spinlock
// Lab3 block if less than zero
// input:  pointer to a counting semaphore
// output: none
void OS_Wait(Sema4Type *semaPt){
  // put Lab 2 (and beyond) solution here
  /*//spinlock
	DisableInterrupts();
	while(semaPt->Value <= 0){
		EnableInterrupts();
		OS_Suspend();		//let other threads have a turn
		DisableInterrupts();
	}
	semaPt->Value = semaPt->Value - 1;
	EnableInterrupts(); */
	
	
	
	/* blocking semaphore */
	uint32_t sr = StartCritical();
	
	semaPt->Value = semaPt->Value - 1;
	
	if(semaPt->Value < 0){
		//block
		TCB* toBlock = unlinkNodeFromActive(RunPt);
		
		if(semaPt->head != NULL){	//checking for empty list
			toBlock->prev = NULL;
			TCB* itr = semaPt->head;
			TCB* follow = NULL;
			uint8_t placed = 0;
			
			while(itr != NULL){
				if(itr->priority > toBlock->priority){
					//place in front because toBlock has 'higher' priority
					toBlock->prev = itr;
					placed = 1;
					
					if(follow == NULL){ //if front of list
						semaPt->head = toBlock;	
					}else{
						follow->prev = toBlock; //maintain link
					}
					
					break;	//finished placing
				}
				
				follow = itr;
				itr = itr->prev;
			}
			
			if(placed == 0){	//if block hasn't been placed
				follow->prev = toBlock;
			}
			
		}else{
			//empty list
			toBlock->prev = NULL;
			semaPt->head = toBlock;
		}
		
		EndCritical(sr);
		OS_Suspend();
	}
	
	/* end of blocking semaphore implementation */	
	EndCritical(sr);
	
	
}; 

// ******** OS_Signal ************
// increment semaphore 
// Lab2 spinlock
// Lab3 wakeup blocked thread if appropriate 
// input:  pointer to a counting semaphore
// output: none
void OS_Signal(Sema4Type *semaPt){
  // put Lab 2 (and beyond) solution here
	/*//spinlock
	uint32_t sr = StartCritical();
	
	semaPt->Value = semaPt->Value + 1;
	
	EndCritical(sr); */

	//blocking
	uint32_t sr = StartCritical();
	
	semaPt->Value = semaPt->Value + 1;
	
	//priority waiting
	if(semaPt->Value <= 0){
		if(semaPt->head != NULL){
			TCB* waking = semaPt->head;	//list is ordered
			semaPt->head = semaPt->head->prev;
			
			addToActive(waking);
			OS_Suspend();
		}
	}
	
	EndCritical(sr);

}; 

// ******** OS_bWait ************
// Lab2 spinlock, set to 0
// Lab3 block if less than zero
// input:  pointer to a binary semaphore
// output: none
void OS_bWait(Sema4Type *semaPt){
  // put Lab 2 (and beyond) solution here
	/*DisableInterrupts();
	while(semaPt->Value == 0){
		EnableInterrupts();
		OS_Suspend();		//let other threads have a turn
		DisableInterrupts();
	}
	semaPt->Value = 0;
	EnableInterrupts(); */
		
	
	OS_Wait(semaPt);
	
}; 

// ******** OS_bSignal ************
// Lab2 spinlock, set to 1
// Lab3 wakeup blocked thread if appropriate 
// input:  pointer to a binary semaphore
// output: none
void OS_bSignal(Sema4Type *semaPt){
  // put Lab 2 (and beyond) solution here

	/*uint32_t sr = StartCritical();
	
	semaPt->Value = 1;
	
	EndCritical(sr); */
	
	OS_Signal(semaPt);
}; 



// ******** SetInitialStack ************
// Initialize the stack of a new TCB
// input:  pointer to TCB
// output: none
void SetInitialStack(TCB *thread){
	thread->stackPtr = &(thread->stack[STACKSIZE - 16]);
	
	thread->stack[STACKSIZE - 1] = 0x01000000; //Thumb bit
	thread->stack[STACKSIZE - 3] = 0x14141414; //R14
	thread->stack[STACKSIZE - 4] = 0x12121212; //R12
	thread->stack[STACKSIZE - 5] = 0x03030303; //R3
	thread->stack[STACKSIZE - 6] = 0x02020202; //R2
	thread->stack[STACKSIZE - 7] = 0x01010101; //R1
	thread->stack[STACKSIZE - 8] = 0x00000000; //R0
	thread->stack[STACKSIZE - 9] = 0x11111111; //R11
	thread->stack[STACKSIZE - 10] = 0x10101010; //R10
	thread->stack[STACKSIZE - 11] = 0x09090909; //R9
	thread->stack[STACKSIZE - 12] = 0x08080808; //R8
	thread->stack[STACKSIZE - 13] = 0x07070707; //R7
	thread->stack[STACKSIZE - 14] = 0x06060606; //R6
	thread->stack[STACKSIZE - 15] = 0x05050505; //R5
	thread->stack[STACKSIZE - 16] = 0x04040404; //R4
}

uint32_t work2 = 0;
uint32_t work1 = 0;
uint32_t dump[300];

//Jitter calculations
//num is the task 0 or 1 and period is in 12.5ns units
void jitterCalc(uint32_t num, uint32_t period){
	static uint32_t LastTime1 = 0;  // time at previous sample
	static uint32_t LastTime2 = 0;  // time at previous sample
	volatile uint32_t thisTime;
	uint32_t jitter;
	
	
	thisTime = OS_Time();       // current time, 12.5 ns
	//currently if else chain, how does this work for long term?
	if(num == 0){
		work1++;
		if(work1 > 1){	// ignore timing of first interrupt
			uint32_t diff = OS_TimeDifference(LastTime1,thisTime);
      if(diff>period){
        jitter = (diff-period+4)/8;  // in 0.1 us 
      }else{
        jitter = (period-diff+4)/8;  // in 0.1 us
      }
			
			dump[work1] = jitter;
			
      if(jitter > MaxJitter){
        MaxJitter = jitter; // in usec
      }       // jitter should be 0
      if(jitter >= JitterSize){
        jitter = JitterSize-1;
      }
      JitterHistogram1[jitter]++; 
		}
		
		LastTime1 = thisTime;
	
	}else if(num == 1){
		work2++;
		if(work2 > 1){	// ignore timing of first interrupt
			uint32_t diff = OS_TimeDifference(LastTime2,thisTime);
      if(diff>period){
        jitter = (diff-period+4)/8;  // in 0.1 usec
      }else{
        jitter = (period-diff+4)/8;  // in 0.1 usec
      }
			
			//dump[work2] = jitter;
			
      if(jitter > MaxJitter2){
        MaxJitter2 = jitter; // in usec
      }       // jitter should be 0
      if(jitter >= JitterSize2){
        jitter = JitterSize2-1;
      }
      JitterHistogram2[jitter]++; 
		}
		
		LastTime2 = thisTime;
		
	}
	
}

uint32_t numThreads = 0;
//******** OS_AddThread *************** 
// add a foregound thread to the scheduler
// Inputs: pointer to a void/void foreground task
//         number of bytes allocated for its stack
//         priority, 0 is highest, 5 is the lowest
// Outputs: 1 if successful, 0 if this thread can not be added
// stack size must be divisable by 8 (aligned to double word boundary)
// In Lab 2, you can ignore both the stackSize and priority fields
// In Lab 3, you can ignore the stackSize fields
int OS_AddThread(void(*task)(void), 
   uint32_t stackSize, uint32_t priority){
  // put Lab 2 (and beyond) solution here
	uint32_t sr = StartCritical();
	TCB *thread = (TCB*) malloc(sizeof(TCB));
	/* checking pid here (CODE IN LATER)
		if (thread->pcb->pid != 0 ){
			//implement adding another child here
		
	}
	
	*/
	if(thread == NULL || priority > 5){	//no more memory for thread, bad priority input
		return 0;
	}
	
	//priority += 3;	//makes sure foreground threads are lower than background 
	thread->priority = priority;
	
	//stack init
	SetInitialStack(thread);
	thread->stack[STACKSIZE - 2] = (long)(task); //PC 
	
	thread->overflowcheck = 0;	
	thread->overflowcheckbot = 0;	
	thread->sleep = 0;	
	thread->blocked = 0;	
	thread->id = numThreads;	
	numThreads++;
	
	if(RunPt == 0 || RunPt->pcb == NULL){
		thread->pcb = NULL;	//!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
	}else{
		thread->pcb = RunPt->pcb;
		
		struct PCB* rPcb = thread->pcb;
		tid_list_item* childTid = (tid_list_item*) malloc(sizeof(tid_list_item));
		childTid->val = thread->id; //setting the tid in pcb
		childTid->text = rPcb->headChildTid->text;
		childTid->data = rPcb->headChildTid->data;
		
		//place holder for just one additional child
		rPcb->headChildTid->nextChild = childTid;
		childTid->nextChild = NULL;
		rPcb->size = rPcb->size + 1;
		
		//??? is this safe for processes added that aren't R9 based
		thread->stack[STACKSIZE - 11] = (long)(childTid->data); 
	}
		 
	//putting in list
	addToActive(thread);
  
	EndCritical(sr);
	
  return 1; // replace this line with solution
};

//******** OS_Id *************** 
// returns the thread ID for the currently running thread
// Inputs: none
// Outputs: Thread ID, number greater than zero 
uint32_t OS_Id(void){
  // put Lab 2 (and beyond) solution here
  
  return RunPt->id; // replace this line with solution
};



void PeriodicTask(void(*task)(void), uint32_t period, uint32_t taskNumber){
	while(1){
		//jitterCalc(taskNumber, period);
		task();
		OS_Sleep(period/SLEEPPERIOD);
	}
}

//******** OS_AddProcess *************** 
// add a process with foregound thread to the scheduler
// Inputs: pointer to a void/void entry point
//         pointer to process text (code) segment
//         pointer to process data segment
//         number of bytes allocated for its stack
//         priority (0 is highest)
// Outputs: 1 if successful, 0 if this process can not be added
// This function will be needed for Lab 5
// In Labs 2-4, this function can be ignored
int OS_AddProcess(void(*entry)(void), void *text, void *data, 
  unsigned long stackSize, unsigned long priority){
  
		uint32_t sr = StartCritical();
	TCB *thread = (TCB*) malloc(sizeof(TCB));
		//initializing values
		thread->id = numThreads;	
		numThreads++;
		thread->pcb = (PCB*) malloc(sizeof(PCB));
		thread->pcb->size = 0;
	/* checking pid here (CODE IN LATER)
		if (thread->pcb->pid != 0 ){
			//implement adding another child here
		
	}
	
	*/
	
		 
	

		
		static int pidCount = 0;
		pidCount++;
		struct PCB* rPcb = thread->pcb;
		rPcb->pid = pidCount; //starts at 1, 2 ,...
		
		
		if ( pidCount > 0  ){ // if child does not have pcb parent
			
			tid_list_item* childTid = (tid_list_item*) malloc(sizeof(tid_list_item));
			childTid->val = thread->id; //setting the tid in pcb
			childTid->text = text;
			childTid->data = data;
			rPcb->headChildTid = childTid;
			rPcb->headChildTid->nextChild = NULL;

		}else{
			return 0; //error for now to check it doesn't use this
			//while(runningThread->head
			//no double children yet...
		}
		
		rPcb->size = rPcb->size + 1;
		thread->pcb = rPcb;
		
	if(thread == NULL || priority > 5){	//no more memory for thread, bad priority input
		return 0;
	}
	
	//priority += 3;	//makes sure foreground threads are lower than background 
	thread->priority = priority;
	
	thread->overflowcheck = 0;
	thread->overflowcheckbot = 0;
	thread->sleep = 0;
	thread->blocked = 0;
	
	//stack init
	SetInitialStack(thread);
	thread->stack[STACKSIZE - 2] = (long)(entry); //PC 
	thread->stack[STACKSIZE - 11] = (long)(data); 
	//putting in list
	addToActive(thread);
	EndCritical(sr);

  return 1; // replace this line with Lab 5 solution
}

//******** OS_AddPeriodicThread *************** 
// add a background periodic task
// typically this function receives the highest priority
// Inputs: pointer to a void/void background function
//         period given in system time units (12.5ns)
//         priority 0 is the highest, 5 is the lowest
// Outputs: 1 if successful, 0 if this thread can not be added
// You are free to select the time resolution for this function
// It is assumed that the user task will run to completion and return
// This task can not spin, block, loop, sleep, or kill
// This task can call OS_Signal  OS_bSignal   OS_AddThread
// This task does not have a Thread ID
// In lab 1, this command will be called 1 time
// In lab 2, this command will be called 0 or 1 times
// In lab 2, the priority field can be ignored
// In lab 3, this command will be called 0 1 or 2 times
// In lab 3, there will be up to four background threads, and this priority field 
//           determines the relative priority of these four threads
int OS_AddPeriodicThread(void(*task)(void), 
   uint32_t period, uint32_t priority){
  // put Lab 2 (and beyond) solution here
/*	uint32_t sr = StartCritical();
	static uint32_t periodThreadNum = 0;
	TCB *thread = (TCB*) malloc(sizeof(TCB));
	
	if(thread == NULL || priority > 5){	//no more memory for thread, bad priority input
		return 0;
	}
	
	thread->priority = 
		 
		 priority;
	
	thread->overflowcheck = 0;
	thread->overflowcheckbot = 0;
	
	//stack init
	SetInitialStack(thread);
	thread->stack[STACKSIZE - 2] = (long)(PeriodicTask); //PC
	//thread->stack[STACKSIZE - 5] = 0x03030303; //R3
	thread->stack[STACKSIZE - 6] = periodThreadNum; //R2
	thread->stack[STACKSIZE - 7] = period; //R1
	thread->stack[STACKSIZE - 8] = (long)(task); //R0	
	
		 
	//putting in list
	addToActive(thread);
	
	periodThreadNum++;
  
	EndCritical(sr);
	
  return 1; */
	/* checking pid here (CODE IN LATER)
		if (thread->pcb->pid != 0 ){
			//implement adding another child here
		
	}
	
	*/
	
	//timer code
	static uint8_t periodicTasks = 0;
  
	uint32_t sr = StartCritical();
		 
	if(priority > 5){
		EndCritical(sr);
		return 0;
	}

	//Doesn't take priority into account rn? OKAY COOL 
	if( periodicTasks == 0){	//WT1A is currently disabled
		Period1 = period;

		WideTimer1A_Init(task, period, priority);	 	//!!!priority things
		periodicTasks++;
	}else if( periodicTasks == 1 ){	//WT1B is currently disabled 
		Period2 = period;

		WideTimer1B_Init( task, period, priority);
		periodicTasks++;
	}else{
		EndCritical(sr);
		return 0;
	}
		
	EndCritical(sr);
	return 1;
};





Sema4Type PF4Set;
Sema4Type PF0Set;

void(*PF4task)(void) = NULL;
void(*PF0task)(void) = NULL;
uint8_t PF4Priority;
uint8_t PF0Priority;

/*----------------------------------------------------------------------------
  PF4 Interrupt Handler
 *----------------------------------------------------------------------------*/
void GPIOPortF_Handler(void){
	
	GPIO_PORTF_ICR_R = 0x11;      // acknowledge flag
	//PF1 ^= 0x02;
	//PF1 ^= 0x02;
	if(!(GPIO_PORTF_DATA_R & 0x00000010) && !(GPIO_PORTF_DATA_R & 0x01)){	//if both switches pressed
		if(PF4Priority > PF0Priority){
			(*PF0task)();
			(*PF4task)();
		}else{
			(*PF4task)();
			(*PF0task)();
		}
	}else if(!(GPIO_PORTF_DATA_R & 0x00000010)){
		(*PF4task)();
	}else if(!(GPIO_PORTF_DATA_R & 0x00000001)){
		(*PF0task)();
	}
	//PF1 ^= 0x02;
	
	/*GPIO_PORTF_ICR_R = 0x10;      // acknowledge flag
	//PF1 ^= 0x02;
	//PF1 ^= 0x02;
	if(!(GPIO_PORTF_DATA_R & 0x00000010)){
		OS_bSignal(&PF4Set);
	}
	if(!(GPIO_PORTF_DATA_R & 0x01)){
		OS_bSignal(&PF0Set);
	}
	//PF1 ^= 0x02; */
 
}

void SW1Task(void(*task)(void)){
	while(1){
		OS_bWait(&PF4Set);
		task();
	}
}

//******** OS_AddSW1Task *************** 
// add a background task to run whenever the SW1 (PF4) button is pushed
// Inputs: pointer to a void/void background function
//         priority 0 is the highest, 5 is the lowest
// Outputs: 1 if successful, 0 if this thread can not be added
// It is assumed that the user task will run to completion and return
// This task can not spin, block, loop, sleep, or kill
// This task can call OS_Signal  OS_bSignal   OS_AddThread
// This task does not have a Thread ID
// In labs 2 and 3, this command will be called 0 or 1 times
// In lab 2, the priority field can be ignored
// In lab 3, there will be up to four background threads, and this priority field 
//           determines the relative priority of these four threads
int OS_AddSW1Task(void(*task)(void), uint32_t priority){
  // put Lab 2 (and beyond) solution here
	
	if(PF4task != NULL){
		return 0;
	}
	
	PF4task = task;	
	PF4Priority = priority;
	
	//portf already initialized in launchpad_init, just need interrupts
	
	GPIO_PORTF_IS_R &= ~0x10;     // (d) PF4 is edge-sensitive
	GPIO_PORTF_IBE_R &= ~0x10;    //     PF4 is not both edges
  GPIO_PORTF_IEV_R &= ~0x10;    //     PF4 falling edge event
  GPIO_PORTF_ICR_R = 0x10;      // (e) clear flag4
  GPIO_PORTF_IM_R |= 0x10;      // (f) arm interrupt on PF4
	
	//NVIC_PRI7_R = (NVIC_PRI7_R&0xFF00FFFF)|((priority + 3)<<21); // priority may need to change when second switch is added
	NVIC_PRI7_R = (NVIC_PRI7_R&0xFF00FFFF)|((5)<<21);
	NVIC_EN0_R = 0x40000000;      // (h) enable interrupt 30 in NVIC
	
  return 1; // replace this line with solution
	
	/*if(!(GPIO_PORTF_IM_R & 0x10)){
		return 0;
	}
	
	uint32_t sr = StartCritical();
	TCB *thread = (TCB*) malloc(sizeof(TCB));
	
	if(thread == NULL){	//no more memory for thread, bad priority input
		return 0;
	}
	
	thread->priority = priority;
	
	thread->overflowcheck = 0;
	thread->overflowcheckbot = 0;
	
	//stack init
	SetInitialStack(thread);
	thread->stack[STACKSIZE - 2] = (long)(SW1Task); //PC
	thread->stack[STACKSIZE - 8] = (long)(task); //R0	
	
		 
	//putting in list
	addToActive(thread);
	
	//portf already initialized in launchpad_init, just need interrupts
	
	GPIO_PORTF_IS_R &= ~0x10;     // (d) PF4 is edge-sensitive
	GPIO_PORTF_IBE_R &= ~0x10;    //     PF4 is not both edges
  GPIO_PORTF_IEV_R &= ~0x10;    //     PF4 falling edge event
  GPIO_PORTF_ICR_R = 0x10;      // (e) clear flag4
  GPIO_PORTF_IM_R |= 0x10;      // (f) arm interrupt on PF4
	
	//NVIC_PRI7_R = (NVIC_PRI7_R&0xFF00FFFF)|((priority + 3)<<21); // priority may need to change when second switch is added
	NVIC_PRI7_R = (NVIC_PRI7_R&0xFF00FFFF)|((5)<<21);
	NVIC_EN0_R = 0x40000000;      // (h) enable interrupt 30 in NVIC

	EndCritical(sr);
	
  return 1; // replace this line with solution */
};

void SW2Task(void(*task)(void)){
	while(1){
		OS_bWait(&PF0Set);
		task();
	}
}

//******** OS_AddSW2Task *************** 
// add a background task to run whenever the SW2 (PF0) button is pushed
// Inputs: pointer to a void/void background function
//         priority 0 is highest, 5 is lowest
// Outputs: 1 if successful, 0 if this thread can not be added
// It is assumed user task will run to completion and return
// This task can not spin block loop sleep or kill
// This task can call issue OS_Signal, it can call OS_AddThread
// This task does not have a Thread ID
// In lab 2, this function can be ignored
// In lab 3, this command will be called will be called 0 or 1 times
// In lab 3, there will be up to four background threads, and this priority field 
//           determines the relative priority of these four threads
int OS_AddSW2Task(void(*task)(void), uint32_t priority){
  // put Lab 2 (and beyond) solution here
	
	if(PF0task != NULL){
		return 0;
	}
	
	PF0task = task;
	PF0Priority = priority;
	
	//portf already initialized in launchpad_init, just need interrupts
	
	GPIO_PORTF_IS_R &= ~0x01;     // (d) PF0 is edge-sensitive
	GPIO_PORTF_IBE_R &= ~0x01;    //     PF0 is not both edges
  GPIO_PORTF_IEV_R &= ~0x01;    //     PF0 falling edge event
  GPIO_PORTF_ICR_R = 0x01;      // (e) clear flag0
  GPIO_PORTF_IM_R |= 0x01;      // (f) arm interrupt on PF4
	
	//NVIC_PRI7_R = (NVIC_PRI7_R&0xFF00FFFF)|((5)<<21); // priority may need to change when second switch is added
	//NVIC_EN0_R = 0x40000000;      // (h) enable interrupt 30 in NVIC
	
  return 1; // replace this line with solution
	
  /*if(!(GPIO_PORTF_IM_R & 0x01)){
		return 0;
	}
	
	uint32_t sr = StartCritical();
	TCB *thread = (TCB*) malloc(sizeof(TCB));
	
	if(thread == NULL){	//no more memory for thread, bad priority input
		return 0;
	}
	
	thread->priority = priority;
	
	thread->overflowcheck = 0;
	thread->overflowcheckbot = 0;
	
	//stack init
	SetInitialStack(thread);
	thread->stack[STACKSIZE - 2] = (long)(SW2Task); //PC
	thread->stack[STACKSIZE - 8] = (long)(task); //R0	
	
		 
	//putting in list
	addToActive(thread);
	
	//portf already initialized in launchpad_init, just need interrupts
	
	GPIO_PORTF_IS_R &= ~0x01;     // (d) PF4 is edge-sensitive
	GPIO_PORTF_IBE_R &= ~0x01;    //     PF4 is not both edges
  GPIO_PORTF_IEV_R &= ~0x01;    //     PF4 falling edge event
  GPIO_PORTF_ICR_R = 0x01;      // (e) clear flag4
  GPIO_PORTF_IM_R |= 0x01;      // (f) arm interrupt on PF4
	
	//NVIC_PRI7_R = (NVIC_PRI7_R&0xFF00FFFF)|((priority + 3)<<21); // priority may need to change when second switch is added
	//NVIC_EN0_R = 0x40000000;      // (h) enable interrupt 30 in NVIC
	
	EndCritical(sr);
	
  return 1; // replace this line with solution */
};


// ******** OS_Sleep ************
// place this thread into a dormant state
// input:  number of msec to sleep
// output: none
// You are free to select the time resolution for this function
// OS_Sleep(0) implements cooperative multitasking
void OS_Sleep(uint32_t sleepTime){
  // put Lab 2 (and beyond) solution here
	
	if(sleepTime == 0)
		OS_Suspend();
	
	uint32_t sr = StartCritical();
	
	TCB* node = RunPt;
	
	unlinkNodeFromActive(node);
	
	if(sleepTime == KILLSTAT){
		node->sleep = sleepTime;
	}else{
		node->sleep = sleepTime + UntouchedTime;
	}
	
	if(SleepHead == NULL){			
		SleepHead = node;						//first thread to be in sleep list
		node->prev = NULL;			//
	}else{
		node->prev = SleepHead;
		SleepHead = node;					//add nodes to start of sleep list
		
	}		
	
	EndCritical(sr);
	
	OS_Suspend();	//!!!!!!!!!!!!! doesn't suspend correctly

};  

// ******** OS_Kill ************
// kill the currently running thread, release its TCB and stack
// input:  none
// output: none
void OS_Kill(void){
  // put Lab 2 (and beyond) solution here
 
	uint32_t sr = StartCritical();
	
	//unlinking from list
	TCB *toKill = unlinkNodeFromActive(RunPt);
	/* unlinking from any pcb holding 
	
	*/
	if(toKill->pcb != NULL && toKill->pcb->pid > 0){ // FOUND PCB associated with thread
		PCB* pcb = toKill->pcb; // set pcb
		struct tid_list_item* itr = pcb->headChildTid;
		struct tid_list_item* follower = NULL;
		while(itr	!= NULL){	//run through size (how many threads)
			if(itr->val == toKill->id){
				pcb->size--;		//update size

				if(pcb->size == 0){
					Heap_Free(itr->data);	
					Heap_Free(itr->text);	
					free(pcb->headChildTid);
					pcb->headChildTid = NULL;
				}else{
					if(follower != NULL){	
						follower->nextChild = itr->nextChild;	
					}else{	
						pcb->headChildTid = itr->nextChild;	
					}
					free(itr);
					break;
				}
				
			}
			
			follower = itr;
			itr = itr->nextChild; //set the next node
				
		}
	}
	
	
	NumCreated--;		//effectively dead thread
	
	EndCritical(sr);
	
	OS_Sleep(KILLSTAT);
    
}; 

// ******** OS_Suspend ************
// suspend execution of currently running thread
// scheduler will choose another thread to execute
// Can be used to implement cooperative multitasking 
// Same function as OS_Sleep(0)
// input:  none
// output: none
void OS_Suspend(void){
  // put Lab 2 (and beyond) solution here
	
  //ContextSwitch();
	
	NVIC_ST_CURRENT_R = 0;      // any write to current clears it
	NVIC_INT_CTRL_R = 0x04000000; // trigger SysTick, for cooperative
};
  
#define FIFOSIZE 512
uint32_t volatile OSPutI;    
uint32_t volatile OSGetI; 
uint32_t static OSFifo [FIFOSIZE];

//Sema4Type DataRoomLeft;
Sema4Type DataAvailable;

Sema4Type Mutex;

// ******** OS_Fifo_Init ************
// Initialize the Fifo to be empty
// Inputs: size
// Outputs: none 
// In Lab 2, you can ignore the size field
// In Lab 3, you should implement the user-defined fifo size
// In Lab 3, you can put whatever restrictions you want on size
//    e.g., 4 to 64 elements
//    e.g., must be a power of 2,4,8,16,32,64,128
void OS_Fifo_Init(uint32_t size){
  // put Lab 2 (and beyond) solution here
  uint32_t sr = StartCritical();                 
	OSPutI = OSGetI = 0;  
		
	OS_InitSemaphore(&DataAvailable, 0);
	OS_InitSemaphore(&Mutex, 1);
	
  EndCritical(sr);   
  
};

// ******** OS_Fifo_Put ************
// Enter one data sample into the Fifo
// Called from the background, so no waiting 
// Inputs:  data
// Outputs: true if data is properly saved,
//          false if data not saved, because it was full
// Since this is called by interrupt handlers 
//  this function can not disable or enable interrupts
int OS_Fifo_Put(uint32_t data){
  // put Lab 2 (and beyond) solution here

	/* working implementation, not good for background tasks
		OS_Wait(&DataRoomLeft);
		OS_Wait(&Mutex);
	
		OSFifo[ OSPutI &(FIFOSIZE-1)] = data; 
		OSPutI++; 

		OS_bSignal(&Mutex);
		OS_Signal(&DataAvailable); */
			
		
	/* old WIP
	OS_bWait(&Mutex);
		
	if((OSPutI - OSGetI) & ~(FIFOSIZE - 1)){
		return 0;
	}else{
		
		OSFifo[ OSPutI &(FIFOSIZE-1)] = data; 
		OSPutI = (OSPutI + 1) % FIFOSIZE;	
		
	} 

	OS_bSignal(&Mutex);
	OS_Signal(&DataAvailable); */
	
	
	
	
	uint32_t sr = StartCritical();
	//OS_Wait(&DataRoomLeft);
	if(((OSPutI + 1) % FIFOSIZE) == OSGetI){
		EndCritical(sr);
		return 0;
	}
	//OS_bWait(&Mutex);
	
	OSFifo[ OSPutI ] = data; 
	OSPutI = (OSPutI + 1) % FIFOSIZE; 

//	OS_bSignal(&Mutex);
	OS_Signal(&DataAvailable);
	
	EndCritical(sr);
	//OS_bSignal(&Mutex);
	//OS_Signal(&DataAvailable);
	
	return 1;    
	
};  

// ******** OS_Fifo_Get ************
// Remove one data sample from the Fifo
// Called in foreground, will spin/block if empty
// Inputs:  none
// Outputs: data 
uint32_t OS_Fifo_Get(void){
  // put Lab 2 (and beyond) solution here
	
  //	working get, not good for background tasks?
	//OS_Wait(&DataAvailable);
	OS_Wait(&DataAvailable);
	OS_bWait(&Mutex);
	
	/*if(OSPutI == OSGetI){
		OS_bSignal(&Mutex);
		
		return 0;
	} */
	
	
	//while(OSPutI == OSGetI){};
	uint32_t sr = StartCritical();
	
	uint32_t data = OSFifo[ OSGetI];
  OSGetI = (OSGetI + 1) % FIFOSIZE;  
	
	EndCritical(sr);
	
	OS_bSignal(&Mutex);
	//OS_Signal(&DataRoomLeft); 
	
	return data;
	
	
	/*uint32_t data = 0;
	
	if(OSPutI == OSGetI){
		return 0;
	}else{
		OS_bWait(&Mutex);
		
		data = OSFifo[ OSGetI &(FIFOSIZE-1)];
		OSGetI = (OSGetI + 1) % FIFOSIZE;  
				
		OS_bSignal(&Mutex);
		
		return data;
	}  */
};

// ******** OS_Fifo_Size ************
// Check the status of the Fifo
// Inputs: none
// Outputs: returns the number of elements in the Fifo
//          greater than zero if a call to OS_Fifo_Get will return right away
//          zero or less than zero if the Fifo is empty 
//          zero or less than zero if a call to OS_Fifo_Get will spin or block
int32_t OS_Fifo_Size(void){
  // put Lab 2 (and beyond) solution here
   return ((unsigned short)( OSPutI -OSGetI ));  \
};


// ******** OS_MailBox_Init ************
// Initialize communication channel
// Inputs:  none
// Outputs: none
void OS_MailBox_Init(void){
  // put Lab 2 (and beyond) solution here
	
	OS_InitSemaphore(&BoxFree, 1);
	OS_InitSemaphore(&DataValid, 0);
	
	Mailbox = 0;

  // put solution here
};

// ******** OS_MailBox_Send ************
// enter mail into the MailBox
// Inputs:  data to be sent
// Outputs: none
// This function will be called from a foreground thread
// It will spin/block if the MailBox contains data not yet received 
void OS_MailBox_Send(uint32_t data){
  // put Lab 2 (and beyond) solution here
  // put solution here
   
	OS_bWait(&BoxFree);
	Mailbox = data;
	OS_bSignal(&DataValid);

};

// ******** OS_MailBox_Recv ************
// remove mail from the MailBox
// Inputs:  none
// Outputs: data received
// This function will be called from a foreground thread
// It will spin/block if the MailBox is empty 
uint32_t OS_MailBox_Recv(void){
  // put Lab 2 (and beyond) solution here
 
	OS_bWait(&DataValid);
	uint32_t data = Mailbox;
	OS_bSignal(&BoxFree);
	
  return data; // replace this line with solution
};

// ******** OS_Time ************
// return the system time 
// Inputs:  none
// Outputs: time in 12.5ns units, 0 to 4294967295
// The time resolution should be less than or equal to 1ms, and the precision 32 bits
// It is ok to change the resolution and precision of this function as long as 
//   this function and OS_TimeDifference have the same resolution and precision 
uint32_t OS_Time(void){
  // put Lab 2 (and beyond) solution here

  //return (systemTime * 80); //
	return (UntouchedTime * 80000);
};

// ******** OS_TimeDifference ************
// Calculates difference between two times
// Inputs:  two times measured with OS_Time
// Outputs: time difference in 12.5ns units 
// The time resolution should be less than or equal to 1ms, and the precision at least 12 bits
// It is ok to change the resolution and precision of this function as long as 
//   this function and OS_Time have the same resolution and precision 
uint32_t OS_TimeDifference(uint32_t start, uint32_t stop){
  // put Lab 2 (and beyond) solution here

  return (stop - start); // replace this line with solution
};


// ******** incrementTime ************
// increments the time variable for the OS
static void incrementTime(){	
	//systemTime++;	//units of us
	
	//if( (systemTime % 1000) == 0 ){
		MsTime++;
		
		UntouchedTime++;
	//}
	//PF2 ^= 0x04;
}

// ******** OS_ClearMsTime ************
// sets the system time to zero (solve for Lab 1), and start a periodic interrupt
// Inputs:  none
// Outputs: none
// You are free to change how this works
// Uses WideTimer 0A
void OS_ClearMsTime(void){
  // put Lab 1 solution here
	MsTime = 0;
	
};

// ******** OS_MsTime ************
// reads the current time in msec (solve for Lab 1)
// Inputs:  none
// Outputs: time in ms units
// You are free to select the time resolution for this function
// For Labs 2 and beyond, it is ok to make the resolution to match the first call to OS_AddPeriodicThread
uint32_t OS_MsTime(void){
  // put Lab 1 solution here
  return MsTime; // replace this line with solution
};



//******** OS_Launch *************** 
// start the scheduler, enable interrupts
// Inputs: number of 12.5ns clock cycles for each time slice
//         you may select the units of this parameter
// Outputs: none (does not return)
// In Lab 2, you can ignore the theTimeSlice field
// In Lab 3, you should implement the user-defined TimeSlice field
// It is ok to limit the range of theTimeSlice to match the 24-bit SysTick
void OS_Launch(uint32_t theTimeSlice){
  // put Lab 2 (and beyond) solution here
  
	//Set Systick period & priority
	NVIC_ST_RELOAD_R = theTimeSlice-1;// reload value 10ms for now
	//NVIC_ST_RELOAD_R = theTimeSlice-1;// reload value for timeslice?
	NVIC_SYS_PRI3_R = (NVIC_SYS_PRI3_R&0x00FFFFFF)|0xC0000000; // priority 6
	
  //set PendSV piority
	NVIC_SYS_PRI3_R = (NVIC_SYS_PRI3_R&0xFF00FFFF)|0x00E00000; //priority 7
	
	//Set RunPt
	RunPt = ActiveHead;
	//Enable interrupts
	//EnableInterrupts();	system enable done in startOS
	
	//branch to user
	StartOS();
  
};

//******** I/O Redirection *************** 
// redirect terminal I/O to UART

int StreamToDevice;	
int fputc (int ch, FILE *f) { 	
  if(StreamToDevice==1){  // Lab 4	
    if(eFile_Write(ch)){          // close file on error	
       OS_EndRedirectToFile(); // cannot write to file	
       return 1;                  // failure	
    }	
    return 0; // success writing	
  }	
  // default UART output
  UART_OutChar(ch);
  return ch; 
}

int fgetc (FILE *f){
  char ch = UART_InChar();  // receive from keyboard
  UART_OutChar(ch);         // echo
  return ch;
}

	/**	
	 * @details open the file for writing, redirect stream I/O (printf) to this file	
	 * @note if the file exists it will append to the end<br>	
	 If the file doesn't exist, it will create a new file with the name	
	 * @param  name file name is an ASCII string up to seven characters	
	 * @return 0 if successful and 1 on failure (e.g., can't open)	
	 * @brief  redirect printf output into this file	
	 */
int OS_RedirectToFile(char *name){
	eFile_Create(name);              // ignore error if file already exists	
	if(eFile_WOpen(name)) return 1;  // cannot open file	
	StreamToDevice = 1;	
	return 0;
}
int OS_RedirectToUART(void){
  StreamToDevice = 0;
  return 1;
}

int OS_RedirectToST7735(void){
  
  return 1;
}

/**	
	 * @details open the file for writing, redirect stream I/O (printf) to this file	
	 * @note if the file exists it will append to the end<br>	
	 If the file doesn't exist, it will create a new file with the name	
	 * @param  name file name is an ASCII string up to seven characters	
	 * @return 0 if successful and 1 on failure (e.g., can't open)	
	 * @brief  redirect printf output into this file	
	 */	
int OS_EndRedirectToFile(void){
	StreamToDevice = 0;	
	if(eFile_WClose()) return 1;	
	return 0;
}
