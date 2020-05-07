#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include "../RTOS_Labs_common/Pong.h"
#include "../inc/ST7735.h"
#include "../RTOS_Labs_common/OS.h"
#include "../inc/WTimer1A.h"

//Globals
#define WIN_SCORE 5
static uint32_t Score[2];
static bool GameEnd;
static Sema4Type ScoreMutex;	//just incase score critical section
static Sema4Type Win;	//tells pong when win has been met

static Sema4Type GoRight;	
static Sema4Type GoLeft;	//step towards closing critical section for player

static Sema4Type PlayerPadMutex;
static Sema4Type AIPadMutex;


ball_t Ball;
paddle_t Paddles[2];	//0 is user 1 is 'AI'

//AI for paddle
static void PaddleAI(void){	
	while(!GameEnd){
		OS_Wait(&AIPadMutex);
		
		ST7735_FillRect(Paddles[1].x, Paddles[1].y, Paddles[1].w, Paddles[1].h, ST7735_BLACK);
		if(Ball.x < Paddles[1].x){	//if there was a way to make AI without relying on the ball (global), we could make multiple balls
			
//			if(rand() % 20 != 0){	
				Paddles[1].x -= 2;
//			}else{	//5% chance of moving other way
//				Paddles[1].x += 2;
//			}
		}
		if((Ball.x + Ball.w) > (Paddles[1].x + Paddles[1].w)){
			
//			if(rand() % 20 != 0){
				Paddles[1].x += 2;
//			}else{
//				Paddles[1].x -= 2;
//			}
		}
		
		if(Paddles[1].x < 0){
				Paddles[1].x = 0;
		}
		if(Paddles[1].x + Paddles[1].w >= ST7735_TFTWIDTH){
				Paddles[1].x = ST7735_TFTWIDTH - Paddles[1].w;
		}
		
		ST7735_FillRect(Paddles[1].x, Paddles[1].y, Paddles[1].w, Paddles[1].h, ST7735_WHITE);
		
		OS_Signal(&AIPadMutex);
		
		OS_Sleep(150);	//a little slower than ball
	}
	
	OS_Kill();
}

//resets ball to middle for now
static void BallReset(void){
	//base size - powerups later?
	Ball.h = BALL_BASE_H;
	Ball.w = BALL_BASE_W;
	
	//place ball in middle-ish
	Ball.x = (ST7735_TFTWIDTH / 2) - Ball.w;	
	Ball.y = (ST7735_TFTHEIGHT / 2) - Ball.h;
	
	Ball.dx = 2 * (rand() % 2) - 1;	//random with 2PAM symbol eq
	Ball.dy = 2 * (rand() % 2) - 1;
	
	 
}


//Game is vertical screen
//X goes 0 -> max
//Y goes 0 v max
static void init(){
	
	//Player paddle initialization
	//base size
	Paddles[0].w = PADDLE_BASE_W;
	Paddles[0].h = PADDLE_BASE_H;
	
	//Location
	Paddles[0].x = (ST7735_TFTWIDTH / 2) - Paddles[0].w;
	Paddles[0].y = ST7735_TFTHEIGHT - Paddles[0].h - PADDLE_PADDING;
	
	//AI paddle
	//base size
	Paddles[1].w = PADDLE_BASE_W;
	Paddles[1].h = PADDLE_BASE_H;
	
	//Location
	Paddles[1].x = (ST7735_TFTWIDTH / 2) - Paddles[1].w;
	Paddles[1].y = PADDLE_PADDING;
	
	//Init score to 0
	Score[0] = 0;
	Score[1] = 0;
	
	//LCD screen
	ST7735_FillScreen(ST7735_BLACK);
	ST7735_FillRect(Paddles[0].x, Paddles[0].y, Paddles[0].w, Paddles[0].h, ST7735_WHITE);
	ST7735_FillRect(Paddles[1].x, Paddles[1].y, Paddles[1].w, Paddles[1].h, ST7735_WHITE);
	
	
}

static bool CollisionCheck(ball_t ball, paddle_t paddle){
	
	uint32_t ball_left = ball.x;
	uint32_t ball_right = ball.x + ball.w;
	uint32_t ball_top = ball.y;
	uint32_t ball_bot = ball.y + ball.h;
	
	uint32_t pad_left = paddle.x;
	uint32_t pad_right = paddle.x + paddle.w;
	uint32_t pad_top = paddle.y;
	uint32_t pad_bot = paddle.y + paddle.h;
	
	if(ball_left > pad_right || ball_right < pad_left) return false;	//if ball isn't in same columns don't bother
	
	//player paddle detection
	if(ball_bot >= pad_top && (ball_bot - pad_top) <= ball.h){
		return true;
	}
	
	//opponent paddle detection
	if(ball_top <= pad_bot && (pad_bot - ball_top) <= 0){
		return true;
	}
	
	return false;
}

//moves the ball and detects collision
static void MoveBall(void){
	
	BallReset();
	
	while(1){
		ST7735_FillRect(Ball.x, Ball.y, Ball.w, Ball.h, ST7735_BLACK);
		
		Ball.x += Ball.dx;
		Ball.y += Ball.dy;
		
//		OS_Wait(&AIPadMutex);
//		OS_Wait(&PlayerPadMutex);
		
		//scoring check
		if(Ball.y < Paddles[1].y){	
			Score[0]++;
			OS_Signal(&ScoreMutex);
			
			break;
		}
		if(Ball.y > (Paddles[0].y + Paddles[0].h)){	
			Score[1]++;
			OS_Signal(&ScoreMutex);
			
			break;
		}
		
		//collision check
		if(Ball.x <= 0 || Ball.x >= ST7735_TFTWIDTH - Ball.w){
			//bounce off side walls
			Ball.dx *= -1;
		}
		if(CollisionCheck(Ball, Paddles[0])){
			Ball.dy *= -1;
		}
		if(CollisionCheck(Ball, Paddles[1])){
			Ball.dy *= -1;
		}
		
//		OS_Signal(&AIPadMutex);
//		OS_Signal(&PlayerPadMutex);
		
		//draw new ball position
		ST7735_FillRect(Ball.x, Ball.y, Ball.w, Ball.h, ST7735_WHITE);
		
		OS_Sleep(125);
	}
	
//	OS_Signal(&AIPadMutex);
//	OS_Signal(&PlayerPadMutex);
	
	OS_Kill();
}

static void UpdateScore(void){
	
	while(1){
		ST7735_SetCursor(0, 0);
		ST7735_OutUDec(Score[0]);
		
		ST7735_SetCursor(19, 0);
		ST7735_OutUDec(Score[1]);
		
		if(Score[0] == WIN_SCORE || Score[1] == WIN_SCORE){
			OS_Signal(&Win);
			OS_Kill();
		}
		
		OS_AddThread(&MoveBall, 512, 2);
		
		OS_Wait(&ScoreMutex);
	}

}

static void PlayerLeft(void){
	while(!GameEnd){
		OS_Wait(&GoLeft);
		
		OS_Wait(&PlayerPadMutex);
		ST7735_FillRect(Paddles[0].x, Paddles[0].y, Paddles[0].w, Paddles[0].h, ST7735_BLACK);
		
		Paddles[0].x -= 2;
		
		if(Paddles[0].x <= 0){
			Paddles[0].x = 0;
		}
		
		ST7735_FillRect(Paddles[0].x, Paddles[0].y, Paddles[0].w, Paddles[0].h, ST7735_WHITE);
		OS_Signal(&PlayerPadMutex);
		
	}	
	
	OS_Kill();
}

static void PlayerRight(void){
	while(!GameEnd){
		OS_Wait(&GoRight);
		
		OS_Wait(&PlayerPadMutex);
		ST7735_FillRect(Paddles[0].x, Paddles[0].y, Paddles[0].w, Paddles[0].h, ST7735_BLACK);
		
		Paddles[0].x += 2;
		
		if(Paddles[0].x + Paddles[0].w >= ST7735_TFTWIDTH){
			Paddles[0].x = ST7735_TFTWIDTH - Paddles[0].w;
		}
		
		ST7735_FillRect(Paddles[0].x, Paddles[0].y, Paddles[0].w, Paddles[0].h, ST7735_WHITE);
		OS_Signal(&PlayerPadMutex);
	}
	
	OS_Kill();
}

static void SignalLeft(){
	OS_Signal(&GoLeft);
}

static void SignalRight(){
	OS_Signal(&GoRight);
}

void Pong(){
	
	init();
	OS_InitSemaphore(&ScoreMutex, 0);
	OS_InitSemaphore(&Win, 0);
	OS_InitSemaphore(&GoLeft, 0);
	OS_InitSemaphore(&GoRight, 0);
	OS_InitSemaphore(&AIPadMutex, 1);
	OS_InitSemaphore(&PlayerPadMutex, 1);
	
	GameEnd = false;
	
	OS_AddSW1Task(&SignalRight, 1);
	OS_AddSW2Task(&SignalLeft, 1);	//double check the buttons!!!!!!
	
	OS_AddThread(&UpdateScore, 512, 2);
	OS_AddThread(&PaddleAI,512, 4);
	OS_AddThread(&MoveBall, 512, 3);
	OS_AddThread(&PlayerRight,512, 1);
	OS_AddThread(&PlayerLeft, 512, 1);	

//	OS_Sleep(120);
//	OS_AddThread(&MoveBall, 512, 3); can't have multiple rn because AI depends on a (global) ball's position
	
	OS_Wait(&Win);
		
	GameEnd = true;
	//game over screen
	ST7735_FillScreen(ST7735_BLACK);
	ST7735_SetCursor(0, 0);
	if(Score[0] == WIN_SCORE){
		ST7735_OutString("You win!!");
	}else{
		ST7735_OutString("You lose :(");
	}
	OS_Kill();
}
