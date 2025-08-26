#include "stm32f10x.h"
#include "option.h"
#include "macro.h"
#include "malloc.h"
#include "lcd.h"
#include "graphics.h"
#include <stdlib.h>

// Led.c

extern void LED_Init(void);
extern void LED_Display(unsigned int num);
extern void LED_All_On(void);
extern void LED_All_Off(void);

// Uart.c

#define Uart_Init			Uart1_Init
#define Uart_Send_Byte		Uart1_Send_Byte
#define Uart_Send_String	Uart1_Send_String
#define Uart_Printf			Uart1_Printf

extern void Uart1_Init(int baud);
extern void Uart1_Send_Byte(char data);
extern void Uart1_Send_String(char *pt);
extern void Uart1_Printf(char *fmt,...);
extern char Uart1_Get_Char(void);
extern char Uart1_Get_Pressed(void);
extern void Uart1_Get_String(char *string);
extern int Uart1_Get_Int_Num(void);
extern void Uart1_RX_Interrupt_Enable(int en);

extern void Uart2_Init(int baud);
extern void Uart2_Send_Byte(char data);
extern void Uart2_Send_String(char *pt);
extern void Uart2_Printf(char *fmt,...);
extern char Uart2_Get_Char(void);
extern char Uart2_Get_Pressed(void);
extern void Uart2_Get_String(char *string);
extern int Uart2_Get_Int_Num(void);
extern void Uart2_RX_Interrupt_Enable(int en);

// Clock.c

extern void Clock_Init(void);

// Key.c

extern void Key_Poll_Init(void);
extern int Key_Get_Pressed(void);
extern void Key_Wait_Key_Released(void);
extern int Key_Wait_Key_Pressed(void);
extern void Key_ISR_Enable(int en);

// Timer.c

extern void TIM2_Delay(int time);
extern void TIM4_Repeat_Interrupt_Enable(int en, int time);

// Asm_Function.s

extern unsigned int __get_IPSR(void);

// Sys_Tick.c

extern void SysTick_OS_Tick(unsigned int msec);

// SysTick.c

extern void SysTick_Run(unsigned int msec);
extern int SysTick_Check_Timeout(void);
extern unsigned int SysTick_Get_Time(void);
extern unsigned int SysTick_Get_Load_Time(void);
extern void SysTick_Stop(void);

// Jog_Key.c

extern void Jog_Poll_Init(void);
extern int Jog_Get_Pressed_Calm(void);
extern int Jog_Get_Pressed(void);
extern void Jog_Wait_Key_Released(void);
extern int Jog_Wait_Key_Pressed(void);
extern void Jog_ISR_Enable(int en);
