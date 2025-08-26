#include "device_driver.h"

typedef struct
{
    uint8_t health;
    uint8_t type;
    int x,y;
	int w,h;
	int ci;
	int dir;
}object_base;

typedef struct
{
    object_base base;
    uint8_t speed;
    uint8_t zombie_case;
    uint8_t attack;
	uint8_t alive;
}zombie;

typedef struct
{
    object_base base;
    uint8_t alarm;
    uint8_t detect_range;
}robot;

typedef struct
{
	object_base base;
	uint8_t life;
	uint8_t speed;
	uint8_t owner; //0 : player, 1 : zombie
}projectile;

#if 0
void zombie_move(void){
    int i;
    for(i=0;i<3;i++){
        if(walker[i].alive){
            walker[i].x += (walker[i].speed*0.5)*walker[i].dir;
            walker[i].y += (walker[i].speed*0.5)*walker[i].dir;

            if(walker[i].x >= X_MAX || walker[i].x <= X_MIN){
                walker[i].dir = -walker[i].dir;
            }

            if(walker[i].y >= Y_MAX || walker[i].y <= Y_MIN){
                walker[i].dir = -walker[i].dir;
            }
        }
    }
}
#endif