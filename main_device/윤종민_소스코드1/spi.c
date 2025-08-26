#include "device_driver.h"

void SPI_SC16IS752_Init(unsigned int div)
{
	int n = 0;

	while(div >>= 1) n++;

	Macro_Set_Bit(RCC->APB2ENR, 3);
	Macro_Write_Block(GPIOB->CRH, 0xffff, 0xb4b3, 16);
	Macro_Set_Bit(GPIOB->ODR, 12);

	Macro_Set_Bit(RCC->APB1ENR, 14);
	SPI2->CR1 = (1<<11)|(0<<10)|(1<<9)|(1<<8)|(0<<7)|((n-1)<<3)|(1<<2)|(0<<1)|(0<<0);
	Macro_Set_Bit(SPI2->CR1, 6);
}

void SPI_SC16IS752_Write_Reg(unsigned int addr, unsigned int data)
{
	volatile int i;

	Macro_Clear_Bit(GPIOB->ODR, 12);
	for(i=0; i<10000; i++);

	SPI2->DR = (0 << 15)|((addr & 0xf) << 11) | (data & 0xff);
	while(Macro_Check_Bit_Clear(SPI2->SR, 1));
	while(Macro_Check_Bit_Set(SPI2->SR, 7));

	for(i=0; i<10000; i++);
	Macro_Set_Bit(GPIOB->ODR, 12);
}

void SPI_SC16IS752_Config_GPIO(unsigned int config)
{
	SPI_SC16IS752_Write_Reg(SC16IS752_IODIR, config);
}

void SPI_SC16IS752_Write_GPIO(unsigned int data)
{
	SPI_SC16IS752_Write_Reg(SC16IS752_IOSTATE, data);
}
