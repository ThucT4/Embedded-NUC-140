#include <stdio.h>
#include "NUC100Series.h"
#include "header.h"

int main(void)
{
	//System initialization start-------------------
	SYS_UnlockReg(); // Unlock protected registers

	//Enable clock sources
	//HXT (12 MHz, HIRC (22 MHz) clock enable
	CLK->PWRCON |= (1 << 0);
	while (!(CLK->CLKSTATUS & (1 << 0)));
	CLK->PWRCON |= (1 << 2);
	while (!(CLK->CLKSTATUS & (1 << 4)));

	//clock source selection
	CLK->CLKSEL0 &=  ~(0b111<<0);
	CLK->CLKSEL0 |= (0b111 << 0);
	//clock frequency division: 1
	CLK->CLKDIV &= ~(0x0F);
	
		//TM3 Clock selection and configuration
	CLK->CLKSEL1 &= ~(0b111 << 20);
	CLK->APBCLK |= (1 << 5);
	//Pre-scale = 239 + 1
	TIMER3->TCSR &= ~(0xFF << 0);
	TIMER3->TCSR |=  0xEF;
		//reset Timer 3
	TIMER3->TCSR |= (1 << 26); 
	//define Timer 3 operation mode
	TIMER3->TCSR &= ~(0b11 << 27);
	TIMER3->TCSR |= (0b01 << 27);
	TIMER3->TCSR &= ~(1 << 24);
	
	//TDR to be updated continuously while timer counter is counting
	TIMER3->TCSR |= (1 << 16);
	TIMER3->TCMPR = 12 - 1;
	TIMER3->TCSR |= (1 << 29);
	TIMER3->TCSR |= (1 << 30);
	
	PB->PMD &= ~(0b11<<22);
	PB->PMD |= (0b01<<22);
	
	SYS_LockReg();  // Lock protected registers
	
	while (1) {
		if(TIMER3->TISR & (1<<0)) {
			PB->DOUT ^= (1<<11);
		TIMER3->TISR |= (1<<0);
		}
	}
}
