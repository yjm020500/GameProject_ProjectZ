#include "device_driver.h"

#define LCDW            (320)
#define LCDH            (240)
#define X_MIN           (0)
#define X_MAX           (LCDW - 1)
#define Y_MIN           (0)
#define Y_MAX           (LCDH - 1)

#define TIMER_PERIOD     (10)
#define BACK_COLOR       (5)
#define DISPLAY_MODE     (3)
#define ARROW_OFFSET      3

//state
#define DEVICE_MENU      (0)
#define SETTING          (1)
#define MUSIC_SELECT     (2)
#define MINI_GAME        (3)
#define MINI_GAME_PLAY   (4)
#define GAME_OVER        (5)
#define GAME_CLEAR       (6)
#define MAIN_DEVICE      (7)

static unsigned short color[] = { RED, YELLOW, GREEN, BLUE, WHITE, BLACK };
const int Device_menu_pos[4][2] = {{10,20},{10,70},{10,120},{10,170}};
const int Game1_Tri_pos[12][2] = {{25,50},{75,50},{125,50},{175,50},{225,50},{275,50},{25,110},{75,110},{125,110},{175,110},{225,110},{275,110}};

extern volatile int TIM4_expired;
extern volatile int Jog_key_in;
extern volatile int Jog_key;
extern volatile int USART2_rx_ready;
extern volatile int USART2_rx_data;

// 시스템 초기화
void System_Init(void)
{
    Clock_Init(); LED_Init(); Key_Poll_Init(); Uart1_Init(115200); Uart2_Init(115200);
    SCB->VTOR  = 0x08003000;
    SCB->SHCSR = 7 << 16;
}

static void T0(int x, int y, int h, unsigned short Color){
    int i, w, xs;
    for(i = 0; i < h; i++){
        w = 2 * i + 1;
        xs = x - w / 2;
        Lcd_Draw_Box(xs, (y - ARROW_OFFSET) + i, w, 1, Color);  // 위로 3칸 올림
    }
}

static void T1(int x, int y, int h, unsigned short Color){
    int i, w, xs;
    for(i = 0; i < h; i++){
        w = 2 * i + 1;
        xs = x - w / 2;
        Lcd_Draw_Box(xs, (y + ARROW_OFFSET) - i, w, 1, Color);  // 아래로 3칸 내림
    }
}

static void T2(int x, int y, int h, unsigned short Color) {
    int i;
    for(i=0;i<h;i++){
        if(i<=h/2){
            Lcd_Draw_Box(x-(int)(1.6*i),y+i,(int)(1.6*i),1,Color);
        }
        else{
            Lcd_Draw_Box(x-(int)(1.6*(h-i)),y+i,(int)(1.6*(h-i)),1,Color);
        }
    }
}
static void T3(int x, int y, int h, unsigned short Color) {
    int i;
    for(i=0;i<h;i++){
        if(i<=h/2){
            Lcd_Draw_Box(x,y+i,(int)(1.6*i),1,Color);
        }
        else{
            Lcd_Draw_Box(x,y+i,(int)(1.6*(h-i)),1,Color);
        }
    }
}
static void Draw_Triangle(int x, int y, int h, int t, unsigned short Color)
{//0:top 1:down 2:left 3:right
    static void (*draw_triangle_func[])(int x, int y, int h, unsigned short Color) = {T0, T1, T2, T3};
	draw_triangle_func[t](x, y, h, Color);
}

static void Move_Triangle(int x, int y, int nx, int ny, int h, unsigned short Color)
{
    Draw_Triangle(x, y, h, 3, BLACK);
    Draw_Triangle(nx, ny, h, 3, Color);
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

static void Print_Device_Menu(void)
{
    Lcd_Clr_Screen();
    Lcd_Printf(40,20,WHITE,BLACK,2,2,"Setting");
    Lcd_Printf(40,70,WHITE,BLACK,2,2,"Hacking");
    Lcd_Printf(40,120,WHITE,BLACK,2,2,"Go to main device");
    Lcd_Printf(40,220,BLUE,BLACK,1,1,"Press SW0 for select");
}

static void Print_Setting(void)
{
    Lcd_Clr_Screen();
    Lcd_Printf(40,20,WHITE,BLACK,2,2,"Music Select");
    Lcd_Printf(40,70,WHITE,BLACK,2,2,"Exit");
    Lcd_Printf(40,220,BLUE,BLACK,1,1,"Press SW0 for select");
}

static void Print_Music_Setting(void)
{
    Lcd_Clr_Screen();
    Lcd_Printf(40,20,WHITE,BLACK,2,2,"Music 1");
    Lcd_Printf(40,70,WHITE,BLACK,2,2,"Music 2");
    Lcd_Printf(40,120,WHITE,BLACK,2,2,"Mute");
    Lcd_Printf(40,170,WHITE,BLACK,2,2,"Exit");
    Lcd_Printf(40,220,BLUE,BLACK,1,1,"Press SW0 for select");
}

static void Print_Mini_Game(void)
{
    Lcd_Clr_Screen();
    Lcd_Printf(40,20,WHITE,BLACK,2,2,"Game 1");
    Lcd_Printf(40,70,WHITE,BLACK,2,2,"Exit");
    Lcd_Printf(40,220,BLUE,BLACK,1,1,"Press SW0 for select");
}

static void Print_Game_Over(void)
{
    Lcd_Clr_Screen();
    Lcd_Printf(85,100,WHITE,BLACK,2,2,"Game Over");
    Lcd_Printf(40,220,BLUE,BLACK,1,1,"Press SW1 for select");
}

static void Print_Game_Clear(void)
{
    Lcd_Clr_Screen();
    Lcd_Printf(80,100,YELLOW,BLACK,2,2,"Game Clear");
    Lcd_Printf(40,220,BLUE,BLACK,1,1,"Press SW1 for select");
}

static void Print_Main_Device(void)
{
    Lcd_Clr_Screen();
    Lcd_Printf(70,110,WHITE,BLACK,2,2,"Main Device");
}

static void Display_clock(int tick)
{
    Lcd_Printf(290,0,WHITE,BLACK,1,1,"%d.%d", tick/10, tick%10);
}

static void Game1_Next(uint8_t * game1_sol)
{
    int i;
    for(i=0;i<6;i++){
        game1_sol[i] = rand()%4;
    }
}

static void Print_Game1_Sol(uint8_t * game1_sol, uint8_t game1_solved)
{
    Lcd_Clr_Screen();
    int i;
    for(i=0;i<6;i++){
        if(game1_sol[i]<=1){
            Draw_Triangle(Game1_Tri_pos[i][0], Game1_Tri_pos[i][1], 15, game1_sol[i], RED);
        }
        else{
            Draw_Triangle(Game1_Tri_pos[i][0], Game1_Tri_pos[i][1], 31, game1_sol[i], RED);
        }
        
    }
}

static void Print_Game1_Player(int input, uint8_t num)
{
    Draw_Triangle(Game1_Tri_pos[num+6][0], Game1_Tri_pos[num+6][1], 15, input, color[0]);
}

static void Clear_Game1_Player(uint8_t * game1_player)
{
    int i;
    for(i=0;i<6;i++)
    {
        Draw_Triangle(Game1_Tri_pos[i+6][0], Game1_Tri_pos[i+6][1], 15, game1_player[i], BLACK);
    }
}

static int game1_player_Is_Cor(uint8_t * game1_sol, uint8_t * game1_player)
{
    int i;
    for(i=0;i<6;i++){
        if(game1_sol[i] != game1_player[i]) return 0;
    }
    return 1;
}

void Main(void)
{
    System_Init();
    srand(TIM4->CNT);
    Uart1_Printf("Zombie Game Start - device\n");

    Lcd_Init(DISPLAY_MODE);
    Jog_Poll_Init(); Jog_ISR_Enable(1); Uart1_RX_Interrupt_Enable(1); Uart2_RX_Interrupt_Enable(1);
    Lcd_Clr_Screen();
    static uint8_t game1_sol[6], game1_player[6];
    static uint8_t state = MAIN_DEVICE;
    static uint8_t rx_stack=0, rx_state;
    static uint8_t key=0, device_menu=0, setting_menu=0, music_select_menu=0, mini_game_menu=0, music_select=3;
    static uint8_t game1_solved = 0, game1_num=0;
    static uint8_t send_flag=1;
    int tick=150;

    TIM4_Repeat_Interrupt_Enable(1,TIMER_PERIOD*10);

    Print_Main_Device();

    for(;;){
        if(USART2_rx_ready){
            Uart1_Printf("%d", USART2_rx_data);
            rx_state = USART2_rx_data/10;
            rx_stack = USART2_rx_data%10;
            if(rx_state == 1) rx_stack = 100;
            key=0;
            state = DEVICE_MENU;
            send_flag=0;
            Print_Device_Menu();
            Draw_Triangle(Device_menu_pos[device_menu][0], Device_menu_pos[device_menu][1], 31, 3, RED);
            USART2_rx_ready = 0;
        }
        if(Jog_key_in){
            if(Jog_key<=3){
                switch (state)
                {
                    case DEVICE_MENU: {
                        device_menu = Move_Jog(Jog_key, device_menu, 3);
                        break;
                    }
                    case SETTING: {
                        setting_menu = Move_Jog(Jog_key, setting_menu, 2);
                        break;
                    }
                    case MUSIC_SELECT: {
                        music_select_menu = Move_Jog(Jog_key, music_select_menu, 4);
                        break;
                    }
                    case MINI_GAME: {
                        mini_game_menu = Move_Jog(Jog_key, mini_game_menu, 2);
                        game1_num = 0;
                        break;
                    }
                    case MINI_GAME_PLAY: {
                        if(game1_num<6){
                            Print_Game1_Player(Jog_key, game1_num);
                            game1_player[game1_num] = Jog_key;
                            game1_num++;
                            break;
                        }
                    }
                    break;
                }
                
            } else if(Jog_key==4){
                switch (state)
                {
                    case DEVICE_MENU: {
                        switch (device_menu){
                            case 0: {
                                Print_Setting(); 
                                Draw_Triangle(Device_menu_pos[0][0],Device_menu_pos[0][1], 31, 3, RED);
                                setting_menu=0;
                                state = SETTING;  
                                break;
                            }
                            case 1: {
                                Print_Mini_Game();
                                Draw_Triangle(Device_menu_pos[0][0],Device_menu_pos[0][1], 31, 3, RED);
                                mini_game_menu=0;
                                state = MINI_GAME; 
                                break;}
                            case 2: state = MAIN_DEVICE; break;
                        }
                        break;
                    }
                    case SETTING: {
                        switch (setting_menu){
                            case 0: {
                                Print_Music_Setting();
                                Draw_Triangle(Device_menu_pos[0][0],Device_menu_pos[0][1], 31, 3, RED);
                                music_select_menu=0;
                                state = MUSIC_SELECT; 
                                break;}
                            case 1: {
                                Print_Device_Menu();
                                Draw_Triangle(Device_menu_pos[0][0],Device_menu_pos[0][1], 31, 3, RED);
                                device_menu=0;
                                state = DEVICE_MENU;
                                break;
                            }
                        }
                        break;
                    }
                    case MUSIC_SELECT: {
                        switch (music_select_menu){
                            music_select=3;
                            case 0: music_select=0; break; //song1 선택
                            case 1: music_select=1; break; //song2 선택
                            case 2: music_select=2; break; //mute
                            case 3: {
                                Print_Setting(); 
                                Draw_Triangle(Device_menu_pos[0][0],Device_menu_pos[0][1], 31, 3, RED);
                                setting_menu=0;
                                state = SETTING;  
                                break;
                            }
                        }
                        break;
                    }
                    case MINI_GAME: {
                        switch (mini_game_menu){
                            case 0: {
                                if(rx_stack>0){
                                    Game1_Next(game1_sol);
                                    game1_solved=0; tick=150; game1_num=0;
                                    Print_Game1_Sol(game1_sol, game1_solved);
                                    state = MINI_GAME_PLAY;
                                }
                                break;
                            }
                            case 1:{
                                Print_Device_Menu();
                                Draw_Triangle(Device_menu_pos[0][0],Device_menu_pos[0][1], 31, 3, RED);
                                device_menu=0;
                                state = DEVICE_MENU;
                                break; 
                            }
                        }
                        break;
                    }
                    case MINI_GAME_PLAY: {
                        Clear_Game1_Player(game1_player);
                        if((game1_num==6) && (game1_player_Is_Cor(game1_sol, game1_player))){
                            game1_solved++;
                            Game1_Next(game1_sol);
                            Print_Game1_Sol(game1_sol, game1_solved);
                            if(game1_solved==3){
                                Print_Game_Clear();
                                state = GAME_CLEAR;
                                rx_stack--;
                                key = (key+1)%10;
                            }
                        }
                        game1_num=0;
                        break;
                    }
                    break;
                }
            }
            else if(Jog_key==5){
                switch (state)
                {
                    case GAME_OVER:{
                        Print_Mini_Game();
                        device_menu=0;
                        Draw_Triangle(Device_menu_pos[device_menu][0], Device_menu_pos[device_menu][1], 31, 3, RED);
                        state = MINI_GAME;
                        break;
                    }
                    case GAME_CLEAR:{
                        Print_Mini_Game();
                        device_menu=0;
                        Draw_Triangle(Device_menu_pos[device_menu][0], Device_menu_pos[device_menu][1], 31, 3, RED);
                        state = MINI_GAME;
                        break;
                    }
                }
            }
            Jog_key_in=0;
        }

        if(TIM4_expired){
            tick--;
            if(state == MINI_GAME_PLAY){
                Display_clock(tick);
                if(tick==0){
                    Print_Game_Over();
                    state = GAME_OVER;
                    tick=150;
                }   
            }

            TIM4_expired=0;
        }

        if(state == MAIN_DEVICE && send_flag==0)
        {
            Print_Main_Device();
            Uart2_Send_Byte(music_select*10+key);
            Uart1_Printf("send%d\n", music_select*10+key);
            send_flag=1;
        }
    }
}

