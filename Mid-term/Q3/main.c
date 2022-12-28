//------------------------------------------- main.c CODE STARTS ---------------------------------------------------------------------------
#include <stdio.h>
#include "NUC100Series.h"

#define SYSTICK_LVR 32768
int main(void)
{
    //System initialization start-------------------
    SYS_UnlockReg();
    //enable clock sources
    CLK->PWRCON |= (1 << 0);
    while(!(CLK->CLKSTATUS & (1 << 0)));
		CLK->PWRCON |= (1 << 1);
		while(!(CLK->CLKSTATUS & (1 << 1)));
    //Select CPU clock
    CLK->CLKSEL0 &= ~(0b111 << 0);
    //CPU clock frequency divider
    CLK->CLKDIV &= ~(0xF);
    //System initialization end---------------------
    
    //GPIO initialization start --------------------
		//Set output push-pull for GPC12 - GPC15 (LED5 - LED8) 
    PC->PMD &= ~(0b11 << 24);
    PC->PMD |= (0b01 << 24);
		//GPIO initialization end ----------------------

    //System Tick initialization start--------------
    //STCLK as SysTick clock source
    SysTick->CTRL &= ~(1 << 2);
		SysTick->CTRL = (1 << 1);
    CLK->CLKSEL0 &= ~(0b111 << 3);
    CLK->CLKSEL0 |= (0b001 << 3);
    
    //SysTick Reload Value
    SysTick->LOAD = SYSTICK_LVR - 1;
    SysTick->VAL = 0;
    //Start SysTick
    SysTick->CTRL |= (1 << 0);
    //System Tick initialization end----------------
    SYS_LockReg();  // Lock protected registers
    
    while(1){}
		}
//------------------------------------------- main.c CODE ENDS ---------------------------------------------------------------------------
void SysTick_Handler() {
		PC->DOUT ^= (1 << 12);
}