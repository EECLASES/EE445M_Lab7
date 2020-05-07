
#define PADDLE_PADDING 10
#define PADDLE_BASE_W 16 
#define PADDLE_BASE_H 4
#define BALL_BASE_W 4 
#define BALL_BASE_H 4

//the ball struct
typedef struct {
	int x, y; //position
	int w,h; //size -- if time for powerups
	int dx, dy; //change in position
} ball_t;

//the paddle struct
typedef struct {
	int x, y; //position
	int w,h; //size -- if time for powerups
} paddle_t;

//runs the game
void Pong(void);


