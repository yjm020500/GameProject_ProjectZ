#include "device_driver.h"

#define LCDW         (320)
#define LCDH         (240)
#define X_MIN          (0)
#define X_MAX          (LCDW - 1)
#define Y_MIN          (0)
#define Y_MAX          (LCDH - 1)

#define FROG_STEP      (10)
#define FROG_SIZE_X      (20)
#define FROG_SIZE_Y      (20)

#define PROJECTILE_SIZE (3)
#define PROJECTILE_SPEED (1)
#define PROJECTILE_LIFE  (15)

#define TIMER_PERIOD   (10)
#define BACK_COLOR       (5)
#define ROBOT_COLOR      (0)
#define DISPLAY_MODE     (3)

#define ZOMBIE_MAX       (40)
#define BULLET_MAX       (25)   /* player bullets */
#define SPIT_MAX         (25)   /* spitter spits */
#define ROBOT_MAX        (3)
#define ITEM_BOX_MAX     (3)
#define MAX_STAGE        (5)

#define Z_WALKER   0
#define Z_WARRIOR  1
#define Z_SPITTER  2
#define Z_RUNNER   3

/*state 정의*/
#define IDLE 0
#define MENU 1
#define SETTING 2
#define MUSIC_SELECT 3
#define PLAY 4
#define DEVICE 5
#define GAME_OVER 6
#define GAME_CLEAR 7

#define MENU_COUNT 3
#define SETTING_MENU_COUNT 2
#define MUSIC_COUNT 1

static unsigned short color[] = { RED, YELLOW, GREEN, BLUE, WHITE, BLACK };
const int Device_menu_pos[4][2] = {{10,20},{10,70},{10,120},{10,170}};
uint8_t state = IDLE;

volatile int song_idx=0;
volatile uint8_t music_select = 0; 

extern volatile int TIM4_expired;
extern volatile int Jog_key_in;
extern volatile int Jog_key;
extern volatile int USART2_rx_ready;
extern volatile int USART2_rx_data;
extern volatile int TIM2_Expired;
extern volatile int TIM3_Expired;

//난이도 table (완화된 버전)
const uint8_t stage_table[5][6] = {
    {  4, 0, 0, 0, 1, 1 },  // Stage 1: walkers 4, robot 1, item 1
    {  5, 1, 0, 0, 1, 1 },  // Stage 2: walkers 5, warrior 1
    {  5, 1, 1, 0, 1, 1 },  // Stage 3: walkers+warrior+spitter 총 7
    {  6, 1, 1, 1, 1, 1 },  // Stage 4: 총 9~10기물, runner 등장
    {  6, 1, 1, 1, 1, 2 }   // Stage 5: item 2개, 총합 10기물 유지
};

// 기본 객체
typedef struct {
    int health;
    uint8_t type;
    int x, y, w, h;
    int ci, dir;
} object_base;

// 좀비 객체
typedef struct {
    object_base base;
    uint8_t speed, zombie_case, attack, alive;
    uint8_t damage_cooldown;
} zombie;

// 발사체 객체
typedef struct {
    object_base base;
    int life;
    uint8_t speed, owner;
} projectile;

typedef struct
{
    object_base base;
    uint8_t detect_range;
}robot;

void System_Init(void)
{
    Clock_Init(); LED_Init(); Key_Poll_Init(); Uart1_Init(115200); Uart2_Init(115200); TIM3_Out_Init(); 
    SPI_SC16IS752_Init(32); // PCLK2(36MHz)/32 = 1.125MHz (주파수 값이 아닌 분주비 전달)
	SPI_SC16IS752_Config_GPIO(0xFF);
    SCB->VTOR  = 0x08003000;
    SCB->SHCSR = 7 << 16;
}

// 사각형 그리기
static void Draw_Object(const object_base *o)
{
    Lcd_Draw_Box(o->x, o->y, o->w, o->h, color[o->ci]);
}

static void Draw_Triangle(int x, int y, int h, unsigned short Color)
{   int i;
    for(i=0;i<h;i++){
        if(i<=h/2){
            Lcd_Draw_Box(x,y+i,(int)(1.6*i),1,Color);
        }
        else{
            Lcd_Draw_Box(x,y+i,(int)(1.6*(h-i)),1,Color);
        }
    }
}

static void Move_Triangle(int x, int y, int nx, int ny, int h, unsigned short Color)
{
    Draw_Triangle(x, y, h, BLACK);
    Draw_Triangle(nx, ny, h, Color);
}

static uint8_t Move_Jog(int Jog_key, uint8_t count, uint8_t max_count)
{
    switch (Jog_key){
        case 0: {
            uint8_t old = count;
            if(count==0) count = max_count-1;
            else count--;
            Move_Triangle(Device_menu_pos[old][0],Device_menu_pos[old][1],Device_menu_pos[count][0],Device_menu_pos[count][1],31,RED);
            break;
        }
        case 1: {
            uint8_t old = count;
            count = (count + 1) % max_count;
            Move_Triangle(Device_menu_pos[old][0],Device_menu_pos[old][1],Device_menu_pos[count][0],Device_menu_pos[count][1],31,RED);
            break;
        }
    }
    return count;
}

// 객체 초기화
static void Object_base_Init(object_base *o, int health, uint8_t type,
                             int x, int y, int w, int h, int ci)
{
    o->health = health; o->type = type;
    o->x = x; o->y = y; o->w = w; o->h = h;
    o->ci = ci; o->dir = 0;
}

static void Print_Idle(void) {
    Lcd_Clr_Screen();
    Lcd_Printf(70, 110,WHITE,BLACK,2,2,"Project Z");
    Lcd_Printf(40,220,BLUE,BLACK,1,1,"Press SW1 for select");
}
static void Print_Menu(void) {
    Lcd_Clr_Screen();
    Lcd_Printf(40,20,WHITE,BLACK,2,2,"Setting");
    Lcd_Printf(40,70,WHITE,BLACK,2,2,"Play Game");
    Lcd_Printf(40,120,WHITE,BLACK,2,2,"Go to Device");
    Lcd_Printf(40,220,BLUE,BLACK,1,1,"Press SW0 for select");
}
static void Print_Setting(void){
    Lcd_Clr_Screen();
    Lcd_Printf(40,20,WHITE,BLACK,2,2,"Music_Select");
    Lcd_Printf(40,70,WHITE,BLACK,2,2,"Exit");
    Lcd_Printf(40,220,BLUE,BLACK,1,1,"Press SW0 for select");
}
static void Print_Music_Setting(void){
    Lcd_Clr_Screen();
    Lcd_Printf(40,20,WHITE,BLACK,2,2,"Music 1");
    Lcd_Printf(40,70,WHITE,BLACK,2,2,"Music 2");
    Lcd_Printf(40,120,WHITE,BLACK,2,2,"Mute");
    Lcd_Printf(40,170,WHITE,BLACK,2,2,"Exit");
    Lcd_Printf(40,220,BLUE,BLACK,1,1,"Press SW0 for select");
}
static void Print_Game_Over(void){
    Lcd_Clr_Screen();
    Lcd_Printf(85,100,WHITE,BLACK,2,2,"Game Over");
    Lcd_Printf(40,220,BLUE,BLACK,1,1,"Press SW1 for select");
}

static void Print_Game_Clear(void){
    Lcd_Clr_Screen();
    Lcd_Printf(80,100,YELLOW,BLACK,2,2,"Game Clear");
    Lcd_Printf(40,220,BLUE,BLACK,1,1,"Press SW1 for select");
}

static void Print_Device(void){
    Lcd_Clr_Screen();
    Lcd_Printf(100,100,WHITE,BLACK,2,2,"Device");
}

// 좀비 소환
static void Summon_Zombie(zombie *z, int case_type)
{
    z->base.x = rand() % ((X_MAX - X_MIN + 1) - 10) + X_MIN; z->base.y = rand() % ((Y_MAX - Y_MIN + 1) - 10) + Y_MIN; 
    z->base.dir = 0; z->alive = 1; z->zombie_case = case_type;
    switch(case_type) {
    case Z_WALKER:
        z->base.health = 2; z->speed = 1; z->base.w = 10; z->base.h = 10; z->base.ci = 1; z->attack = 0; z->damage_cooldown=0; break;
    case Z_WARRIOR:
        z->base.health = 5; z->speed = 1; z->base.w = 15; z->base.h = 15; z->base.ci = 2; z->attack = 0; z->damage_cooldown=0; break;
    case Z_SPITTER:
        z->base.health = 2; z->speed = 1; z->base.w = 10; z->base.h = 10; z->base.ci = 3; z->attack = 1; z->damage_cooldown=0; break;
    case Z_RUNNER:
        z->base.health = 1; z->speed = 2; z->base.w = 10; z->base.h = 10; z->base.ci = 4; z->attack = 0; z->damage_cooldown=0; break;
    }
    Draw_Object(&z->base);
}

// 발사체 초기화
static void Projectile_Init(projectile *p, int x, int y, int dir, uint8_t owner, uint8_t ci)
{
    Object_base_Init(&p->base, 1, 5, x, y, PROJECTILE_SIZE, PROJECTILE_SIZE, ci);
    p->base.dir = dir;
    p->life = PROJECTILE_LIFE; p->speed = PROJECTILE_SPEED; p->owner = owner;
}

//로봇 초기화
static void Robot_Init(robot * r, uint8_t ci, uint8_t detect_range)
{
    Object_base_Init(&r->base, 100, 3, 10*(rand()%10), 15*(rand()%10), FROG_SIZE_X, FROG_SIZE_Y, ci);
    r->detect_range = detect_range;
    Draw_Object(&r->base);
}

//아이템박스 초기화
static void Item_Box_Init(object_base * i, uint8_t ci)
{
    Object_base_Init(i, 100, 4, 20*(rand()%10), 20*(rand()%10), 10, 10, ci);
    Draw_Object(i);
}

static void Load_Stage(uint8_t cur_stage, object_base * player, zombie * zombies, robot * robots, object_base * item_boxs)
{
    int i, idx=0;
    // player init
    Object_base_Init(player,3,1,X_MAX/2,Y_MAX/2,FROG_SIZE_X,FROG_SIZE_Y,0);
    Draw_Object(player);
    for(i=0;i<stage_table[cur_stage-1][0];i++) Summon_Zombie(&zombies[idx++],Z_WALKER);  //walker
    for(i=0;i<stage_table[cur_stage-1][1];i++) Summon_Zombie(&zombies[idx++],Z_WARRIOR); //warrior
    for(i=0;i<stage_table[cur_stage-1][2];i++) Summon_Zombie(&zombies[idx++],Z_SPITTER); //spitter
    for(i=0;i<stage_table[cur_stage-1][3];i++) Summon_Zombie(&zombies[idx++],Z_RUNNER);  //runner
    for(i=0;i<stage_table[cur_stage-1][4];i++) Robot_Init(&robots[i],0,20);//robots
    for(i=0;i<stage_table[cur_stage-1][5];i++) Item_Box_Init(&item_boxs[i],4);//item_box
}

static void Clear_Entities(zombie *zombies, int zmax, projectile *bullets, int bmax, projectile *spits, int smax,
    robot *robots, int rmax, object_base *items, int imax)
{
    int i;
    for(i=0; i<zmax; i++){
        Lcd_Draw_Box(zombies[i].base.x, zombies[i].base.y, zombies[i].base.w, zombies[i].base.h, color[BACK_COLOR]);
        zombies[i].alive = 0;
        zombies[i].base.health = 0;
    }
    for(i=0; i<bmax; i++){
        Lcd_Draw_Box(bullets[i].base.x, bullets[i].base.y, bullets[i].base.w, bullets[i].base.h, color[BACK_COLOR]);
        bullets[i].base.health = 0;
        bullets[i].life = 0;
    }
    for(i=0; i<smax; i++){
        Lcd_Draw_Box(spits[i].base.x, spits[i].base.y, spits[i].base.w, spits[i].base.h, color[BACK_COLOR]);
        spits[i].base.health = 0;
        spits[i].life = 0;
    }
    for(i=0; i<rmax; i++){
        Lcd_Draw_Box(robots[i].base.x, robots[i].base.y, robots[i].base.w, robots[i].base.h, color[BACK_COLOR]);
        robots[i].base.health = 0;
    }
    for(i=0; i<imax; i++){
        Lcd_Draw_Box(items[i].x, items[i].y, items[i].w, items[i].h, color[BACK_COLOR]);
        items[i].health = 0;
    }
}

static void Game_Over(zombie * zombies, projectile * bullets, projectile * spits, robot * robots, object_base * item_boxs)
{
    Clear_Entities(zombies, ZOMBIE_MAX, bullets, BULLET_MAX, spits, SPIT_MAX, robots, ROBOT_MAX, item_boxs, ITEM_BOX_MAX);
    Lcd_Clr_Screen();
    Print_Game_Over();
    state = GAME_OVER;
}

static void Game_Clear(zombie * zombies, projectile * bullets, projectile * spits, robot * robots, object_base * item_boxs)
{
    Clear_Entities(zombies, ZOMBIE_MAX, bullets, BULLET_MAX, spits, SPIT_MAX, robots, ROBOT_MAX, item_boxs, ITEM_BOX_MAX);
    Lcd_Clr_Screen();
    Print_Game_Clear();
    state = GAME_CLEAR;
}

static int Try_Move_Player(object_base *obj, int dir) {
    int nx = obj->x;
    int ny = obj->y;
    switch(dir) {
      case 0: ny -= FROG_STEP; break;
      case 1: ny += FROG_STEP; break;
      case 2: nx -= FROG_STEP; break;
      case 3: nx += FROG_STEP; break;
    }
    // 새 좌표가 화면 밖이면 “종료 시도”
    if (nx < X_MIN || nx + obj->w > X_MAX ||
        ny < Y_MIN || ny + obj->h > Y_MAX) {
        return 1;
    }
    // 화면 안이라면 실제 이동
    obj->ci = BACK_COLOR; Draw_Object(obj);
    obj->x = nx;  obj->y = ny;
    obj->dir = dir; obj->ci = 0; Draw_Object(obj);
    return 0;
}

// 사각 충돌 검사
static int Check_Collision(const object_base *a, const object_base *b)
{
    return (a->x < b->x + b->w && a->x + a->w > b->x &&
            a->y < b->y + b->h && a->y + a->h > b->y);
}

// 픽셀-사각 검사
static int check_collision_pixel(int px, int py, int x, int y, int w, int h)
{
    return (px >= x && px < x + w && py >= y && py < y + h);
}

// 좀비 이동 (픽셀 복원 + 10틱마다 이동 + 재그리기)
static int Move_Zombie(zombie *z, object_base *player, zombie *zs, int zmax, int tick)
{
    if (!z->alive || z->base.health==0) return 0;
    if (tick % 10 != 0) { Draw_Object(&z->base); return 0; }
    if (z->damage_cooldown > 0) z->damage_cooldown--;
    else if (Check_Collision(&z->base, player) && z->damage_cooldown == 0){
        player->health--;
        if (player->health == 0) return 1;
        z->damage_cooldown = 20;
    }

    // 1) 복원
    { int i,j,k,px,py; unsigned short r;
      for(i=0;i<z->base.w;i++) for(j=0;j<z->base.h;j++){
        px=z->base.x+i; py=z->base.y+j;
        if(px<0||px>=LCDW||py<0||py>=LCDH) continue;
        r = color[BACK_COLOR];
        for(k=0;k<zmax;k++) if(&zs[k]!=z && zs[k].alive &&
             check_collision_pixel(px,py,zs[k].base.x,zs[k].base.y,zs[k].base.w,zs[k].base.h))
            { r=color[zs[k].base.ci]; break; }
        if(k==zmax && check_collision_pixel(px,py,player->x,player->y,player->w,player->h))
            r=color[player->ci];
        Lcd_Put_Pixel(px,py,r);
      }
    }
    // 2) 이동
    if (player->x < z->base.x) z->base.x -= z->speed;
    else if (player->x > z->base.x) z->base.x += z->speed;
    if (player->y < z->base.y) z->base.y -= z->speed;
    else if (player->y > z->base.y) z->base.y += z->speed;
    if (z->base.x < X_MIN) z->base.x = X_MIN;
    if (z->base.x+z->base.w > X_MAX) z->base.x = X_MAX-z->base.w;
    if (z->base.y < Y_MIN) z->base.y = Y_MIN;
    if (z->base.y+z->base.h > Y_MAX) z->base.y = Y_MAX-z->base.h;
    // 3) 그리기
    Draw_Object(&z->base);
    return 0;
}

// 발사체 이동 + 복원 + 충돌 처리
static void Move_Projectile(projectile *p, object_base *player, zombie *zs, int zmax, robot *robots, object_base *items)
{
    if (p->life<=0||p->base.health==0) return;

    // 1) 복원
    { int i,j,k,px,py; unsigned short r;
      for(i=0;i<p->base.w;i++) for(j=0;j<p->base.h;j++){
        px=p->base.x+i; py=p->base.y+j;
        if(px<0||px>=LCDW||py<0||py>=LCDH) continue;
        r = color[BACK_COLOR];
        for(k=0;k<zmax;k++) if(zs[k].alive &&
             check_collision_pixel(px,py,zs[k].base.x,zs[k].base.y,zs[k].base.w,zs[k].base.h))
            { r=color[zs[k].base.ci]; break; }
        if(k==zmax && check_collision_pixel(px,py,player->x,player->y,player->w,player->h))
            r=color[player->ci];
        for(k = 0; k < ROBOT_MAX; k++) {
            if(robots[k].base.health &&
               check_collision_pixel(px, py, robots[k].base.x, robots[k].base.y,
                                     robots[k].base.w, robots[k].base.h)) {
                r = color[robots[k].base.ci];
                break;
            }
        }
        for(k = 0; k < ITEM_BOX_MAX; k++) {
            if(items[k].health &&
               check_collision_pixel(px, py, items[k].x, items[k].y,
                                     items[k].w, items[k].h)) {
                r = color[items[k].ci];
                break;
            }
        }
        Lcd_Put_Pixel(px,py,r);
      }
    }
    // 2) 충돌
    if (p->owner==0) { // player bullet → zombie
        int z;
        for(z=0;z<zmax;z++) if(zs[z].alive &&
            Check_Collision(&p->base,&zs[z].base))
        {
            if(--zs[z].base.health<=0){
                zs[z].alive=0;
                // 죽은 좀비 자리 클리어
                Lcd_Draw_Box(zs[z].base.x,zs[z].base.y, zs[z].base.w,zs[z].base.h, color[BACK_COLOR]);
                // 겹친 플레이어 복원
                if(Check_Collision(player,&zs[z].base)) Draw_Object(player);
            }
            p->life=0; p->base.health=0;
            return;
        }
    } else { // spit → player
        if(Check_Collision(&p->base,player)){
            player->health--;
            p->life=0; p->base.health=0;
            return;
        }
    }
    // 3) 이동
    switch(p->base.dir){
    case 0: p->base.y-=p->speed; break;
    case 1: p->base.y+=p->speed; break;
    case 2: p->base.x-=p->speed; break;
    case 3: p->base.x+=p->speed; break;
    }
    // 4) life/경계
    p->life--;
    if(p->life<=0 || p->base.x<X_MIN||p->base.x>X_MAX||p->base.y<Y_MIN||p->base.y>Y_MAX){
        p->base.health=0;
        return;
    }
    // 5) 그리기
    Draw_Object(&p->base);
}

static void Move_Random(object_base * o, robot * robots, object_base * player)
{
    if(o->health)
    {
        int i,j,px,py,k;
        unsigned short r;

        // 배경 복원
        for(i=0;i<o->w;i++) for(j=0;j<o->h;j++){
            px = o->x + i;
            py = o->y + j;
            if(px<0 || px>=LCDW || py<0 || py>=LCDH) continue;

            r = color[BACK_COLOR];

            // robot 복원 우선
            for(k=0;k<ROBOT_MAX;k++){
                if(robots[k].base.health &&
                   check_collision_pixel(px, py, robots[k].base.x, robots[k].base.y, robots[k].base.w, robots[k].base.h)){
                    r = color[robots[k].base.ci];
                    break;
                }
            }

            // 플레이어 복원
            if(k==ROBOT_MAX && check_collision_pixel(px, py, player->x, player->y, player->w, player->h)){
                r = color[player->ci];
            }

            Lcd_Put_Pixel(px, py, r);
        }

        // 실제 이동
        int dir = rand()%4;
        switch(dir){
            case 0: o->y-=2; if(o->y<Y_MIN) o->y+=4;break;
            case 1: o->y+=2; if(o->y+o->h>Y_MAX) o->y-=4; break;
            case 2: o->x-=2; if(o->x<X_MIN) o->x+= 4; break;
            case 3: o->x+=2; if(o->x+o->w>X_MAX) o->x-=4; break;
        }

        o->ci = 0;
        Draw_Object(o);
    }
}

static void Move_Robot(robot * r, object_base * player)
{
    if(r->base.health)
    {
        if(Check_Collision(&r->base, player)){
            int i,j;
            int px,py;
            for (i = 0; i < r->base.w; i++) {
                for (j = 0; j < r->base.h; j++) {
                    px = r->base.x + i;
                    py = r->base.y + j;
                    if (px < 0 || px >= LCDW || py < 0 || py >= LCDH) continue;
        
                    // 플레이어 영역 위라면 플레이어 색으로, 아니면 배경색으로
                    if (check_collision_pixel(px, py,
                                              player->x, player->y,
                                              player->w, player->h)) {
                        Lcd_Put_Pixel(px, py, color[player->ci]);
                    } else {
                        Lcd_Put_Pixel(px, py, color[BACK_COLOR]);
                    }
                }
            }
        }
        else {
            r->base.ci = BACK_COLOR; 
            Draw_Object(&r->base);
        }

        int dir = rand()%4;
        switch(dir){
            case 0: r->base.y-=2; if(r->base.y<Y_MIN) r->base.y+=4;break;
            case 1: r->base.y+=2; if(r->base.y+r->base.h>Y_MAX) r->base.y-=4; break;
            case 2: r->base.x-=2; if(r->base.x<X_MIN) r->base.x+= 4; break;
            case 3: r->base.x+=2; if(r->base.x+r->base.w>X_MAX) r->base.x-=4; break;
        }

        r->base.ci = ROBOT_COLOR;
        Draw_Object(&r->base);
    }
}

static int check_robot(robot * r, object_base * player)
{
    int x=r->base.x,y=r->base.y,w=r->base.w,h=r->base.h;
    r->base.x=x-r->detect_range, r->base.y = y-r->detect_range, r->base.w = w+r->detect_range*2, r->base.h = h+r->detect_range*2;
    if(Check_Collision(player, &r->base)){
        r->base.x=x, r->base.y = y, r->base.w = w, r->base.h = h;
        return 1;
    }
    r->base.x=x, r->base.y = y, r->base.w = w, r->base.h = h;
    return 0;
}

static void Redraw_Play_Screen(
    const object_base *player,
    const zombie *zombies,
    const robot *robots,
    const object_base *item_boxs,
    const projectile *bullets,
    const projectile *spits)
{
    Lcd_Clr_Screen();

    Draw_Object(player);
    int i;

    for (i = 0; i < ZOMBIE_MAX; i++)
        if (zombies[i].alive) Draw_Object(&zombies[i].base);

    for (i = 0; i < ROBOT_MAX; i++)
        if (robots[i].base.health > 0) Draw_Object(&robots[i].base);

    for (i = 0; i < ITEM_BOX_MAX; i++)
        if (item_boxs[i].health > 0) Draw_Object(&item_boxs[i]);

    for (i = 0; i < BULLET_MAX; i++)
        if (bullets[i].base.health > 0) Draw_Object(&bullets[i].base);

    for (i = 0; i < SPIT_MAX; i++)
        if (spits[i].base.health > 0) Draw_Object(&spits[i].base);
}

static void Update_Health_UI(int health)
{
    switch(health) {
        case 3: SPI_SC16IS752_Write_GPIO(0xF8); break; // 11111000
        case 2: SPI_SC16IS752_Write_GPIO(0xF9); break; // 11111001
        case 1: SPI_SC16IS752_Write_GPIO(0xFB); break; // 11111011
        case 0: SPI_SC16IS752_Write_GPIO(0xFF); break; // 전부 꺼짐
    }
}

void Main(void)
{
    System_Init();
    srand(TIM4->CNT);
    Uart_Printf("Game Project\n");

    Lcd_Init(DISPLAY_MODE);
    Jog_Poll_Init();
    Jog_ISR_Enable(1);
    Uart1_RX_Interrupt_Enable(1);
    Uart2_RX_Interrupt_Enable(1);
    Lcd_Clr_Screen();

    static object_base player;
    static zombie zombies[ZOMBIE_MAX];
    static projectile bullets[BULLET_MAX], spits[SPIT_MAX];
    static robot robots[ROBOT_MAX];
    static object_base item_boxs[ITEM_BOX_MAX];
    static uint8_t item_stack = 0, spawned = 0, is_over=0;
    static uint8_t robot_count, item_box_count;
    static uint8_t cur_stage=1, exit_stage=0, key_received=0, b_idx=0, s_idx=0;
    int i, z;
    uint8_t s = cur_stage-1;
    int tick_count = 0;
    uint8_t menu = 0;
    uint8_t setting_menu = 0;
    uint8_t before_device = 0;
    uint8_t music_setting = 0;
    static int prev_health = -1; // spi 초기값: 모두 꺼짐
    
    SPI_SC16IS752_Write_GPIO(0xFF);  // LED 전부 OFF (처음엔 꺼진 상태로)
    TIM4_Repeat_Interrupt_Enable(1, TIMER_PERIOD * 10);
    TIM2_Interrupt_Delay(1);
    Print_Idle();

    for (;;)
    {//음악 바꿀건지, music_select, key_received
        if(USART2_rx_ready)
        {
            Uart1_Printf("rx_ready = 1, data = %d\n", USART2_rx_data);
            state = before_device;
            if(((USART2_rx_data/10) != music_select)&&((USART2_rx_data/10) != 3)) {music_select = (USART2_rx_data/10); TIM3_Out_Stop(); song_idx=0;}
            switch (before_device){
                case MENU: {
                    Print_Menu();
                        menu=0;
                        Draw_Triangle(Device_menu_pos[menu][0], Device_menu_pos[menu][1], 31, RED);
                        break;
                }
                case PLAY: {
                    if(USART2_rx_data%10){
                        key_received=1;
                    }
                    Redraw_Play_Screen(&player, zombies, robots, item_boxs, bullets, spits);
                    break;
                }
            }
            USART2_rx_ready=0;
        }

        if (Jog_key_in)
        {
            Uart1_Printf("JogKey In Detected: %d, State: %d\n", Jog_key, state);
            if(Jog_key<=3)
            {
                switch (state)
                {
                    case MENU: {
                        menu = Move_Jog(Jog_key, menu, 3);
                        break;
                    }
                    case SETTING: {
                        setting_menu = Move_Jog(Jog_key, setting_menu, 2);
                        break;
                    }
                    case MUSIC_SELECT: {
                        music_setting = Move_Jog(Jog_key, music_setting, 4);
                        break;
                    }
                    case PLAY: {
                        // 이동 시도 — 화면 밖이면 1 반환
                        if (Try_Move_Player(&player, Jog_key)) {
                            exit_stage=1;
                        }
                        // 겹친 좀비 복원
                        for(z=0;z<ZOMBIE_MAX;z++) if(zombies[z].alive) Draw_Object(&zombies[z].base);
                        //itembox겹침
                        for(i = 0; i < item_box_count; i++) {
                            if(Check_Collision(&player, &item_boxs[i]) && item_boxs[i].health) {
                                item_boxs[i].health = 0; 
                                item_stack++;
                                item_boxs[i].ci = BACK_COLOR;
                                Draw_Object(&item_boxs[i]);
                                Draw_Object(&player);
                            }
                        }
                        break;
                    }
                }
            }
            else if(Jog_key==4)
            {
                switch (state)
                {
                    case MENU: 
                    {
                        switch (menu){
                            case 0: {
                                Print_Setting(); 
                                Draw_Triangle(Device_menu_pos[0][0],Device_menu_pos[0][1], 31, RED);
                                setting_menu = 0;
                                state = SETTING;  
                                break;
                            }
                            case 1: {
                                s = cur_stage - 1;
                                robot_count = stage_table[s][4];
                                item_box_count = stage_table[s][5];
                                Clear_Entities(zombies, ZOMBIE_MAX, bullets, BULLET_MAX, spits, SPIT_MAX, robots,  robot_count, item_boxs, item_box_count);
                                Lcd_Clr_Screen();
                                b_idx=0, s_idx=0; key_received=0;
                                Load_Stage(cur_stage, &player, zombies, robots, item_boxs);
                                state = PLAY;
                                break;
                            }
                            case 2:{
                                Print_Device();
                                Uart2_Send_Byte(MENU*10);
                                before_device = MENU;
                                state = DEVICE;
                                break;
                            }
                        }
                        break;
                    }
                    case SETTING: 
                    {
                        switch (setting_menu){
                            case 0: {
                                Print_Music_Setting();
                                Draw_Triangle(Device_menu_pos[0][0],Device_menu_pos[0][1], 31, RED);
                                music_setting=0;
                                state = MUSIC_SELECT; 
                                break;
                            }
                            case 1: {
                                Print_Menu();
                                Draw_Triangle(Device_menu_pos[0][0],Device_menu_pos[0][1], 31, RED);
                                menu=0;
                                state = MENU; 
                                break;
                            }
                        }
                        break;
                    }
                    case MUSIC_SELECT: {
                        switch (music_setting){
                            case 0: music_select = 0; break; //song1 선택
                            case 1: music_select = 1; break; //song2 선택
                            case 2: music_select = 2; break; //mute
                            case 3: {
                                Print_Setting(); 
                                Draw_Triangle(Device_menu_pos[0][0],Device_menu_pos[0][1], 31, RED);
                                setting_menu=0;
                                state = SETTING;  
                                break;
                            }
                        }
                        break;
                    }
                    case PLAY: {
                        Projectile_Init(&bullets[b_idx],
                            player.x+player.w/2, player.y+player.h/2,
                            player.dir, 0, 2);
                        Draw_Object(&bullets[b_idx].base);
                        b_idx=(b_idx+1)%BULLET_MAX;
                        break;
                    }
                }
            }
            else if(Jog_key==5)
            {
                switch (state)
                {
                    case IDLE: {
                        Print_Menu();
                        menu=0;
                        Draw_Triangle(Device_menu_pos[menu][0], Device_menu_pos[menu][1], 31, RED);
                        state = MENU;
                        break;
                    }
                    case PLAY: {
                        Uart1_Printf("Go go the Device!!\n");
                        key_received=0;
                        before_device = PLAY;
                        state = DEVICE;
                        Print_Device();
                        Uart2_Send_Byte((PLAY*10)+item_stack);
                        break;
                    }
                    case GAME_OVER: {
                        Print_Menu();
                        menu=0;
                        Draw_Triangle(Device_menu_pos[menu][0], Device_menu_pos[menu][1], 31, RED);
                        state = MENU;
                        break;
                    }
                    case GAME_CLEAR: {
                        Update_Health_UI(0);
                        Print_Menu();
                        menu=0;
                        Draw_Triangle(Device_menu_pos[menu][0], Device_menu_pos[menu][1], 31, RED);
                        state = MENU;
                        break;
                    }
                }
            }
            Jog_key_in = 0;
        }

        if (TIM4_expired)
        {
            tick_count = (tick_count + 1) % 10;

            // 체력 UI 업데이트
            if (state == PLAY && player.health != prev_health) {
                Update_Health_UI(player.health);
                prev_health = player.health;
            }
            else if (state != PLAY && prev_health != -1) {
                SPI_SC16IS752_Write_GPIO(0xFF);  // LED OFF
                prev_health = -1;
            }


            if(state == PLAY)
            {
                //itembox
                for(z=0;z<item_box_count;z++) Move_Random(&item_boxs[z], robots, &player);
                // 좀비
                for(z=0;z<ZOMBIE_MAX;z++) {
                    is_over = Move_Zombie(&zombies[z],&player,zombies,ZOMBIE_MAX,tick_count);
                    if(is_over) Game_Over(zombies, bullets, spits, robots, item_boxs);
                }
                // 스피터 독액 발사
                if(tick_count==0){
                    //robot
                    for(z=0;z<ROBOT_MAX;z++) Move_Robot(&robots[z], &player);
                    for(z=0;z<ZOMBIE_MAX;z++){
                        if(zombies[z].alive && zombies[z].attack){
                            int dx=player.x-zombies[z].base.x, dy=player.y-zombies[z].base.y;
                            int dir=(abs(dx)>abs(dy)? (dx>0?3:2) : (dy>0?1:0));
                            Projectile_Init(&spits[s_idx],
                                            zombies[z].base.x+zombies[z].base.w/2,
                                            zombies[z].base.y+zombies[z].base.h/2,
                                            dir, 1, 4);
                            Draw_Object(&spits[s_idx].base);
                            s_idx=(s_idx+1)%SPIT_MAX;
                        }
                    }
                }
                // player bullets
                for(i=0;i<BULLET_MAX;i++) if(bullets[i].base.health) Move_Projectile(&bullets[i],&player,zombies,ZOMBIE_MAX, robots, item_boxs);
                // spits
                for(i=0;i<SPIT_MAX;i++) if(spits[i].base.health) Move_Projectile(&spits[i],&player,zombies,ZOMBIE_MAX, robots, item_boxs);
            }

            TIM4_expired = 0;
        }

        if(check_robot(&robots[0], &player)&&(!spawned)&&(state==PLAY)){
            if(spawned!=2){
                Uart1_Printf("sensored!!\n");
                for(z=ZOMBIE_MAX-10;z<ZOMBIE_MAX;z++){
                    Summon_Zombie(&zombies[z],Z_RUNNER);
                    Draw_Object(&zombies[z].base);    
                }
            }
            spawned=2;
        }

        if(exit_stage && key_received && (state==PLAY)){
            if (cur_stage < MAX_STAGE) {
                cur_stage++; s = cur_stage-1;
                robot_count = stage_table[s][4];
                item_box_count = stage_table[s][5];
                Clear_Entities(zombies, ZOMBIE_MAX, bullets, BULLET_MAX, spits, SPIT_MAX, robots, robot_count, item_boxs, item_box_count);
                Lcd_Clr_Screen();
                b_idx=0; s_idx=0;
                Load_Stage(cur_stage, &player, zombies, robots, item_boxs);
                state = PLAY;
            } else {
                Game_Clear(zombies, bullets, spits, robots, item_boxs);
                cur_stage=1;
            }
            exit_stage=0; key_received=0; spawned = 0; item_stack=0;
        }
    }
}