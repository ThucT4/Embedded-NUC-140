//------------------------------------------- main.c CODE STARTS ---------------------------------------------------------------------------
#include <stdio.h>
#include "NUC100Series.h"
#include "interrupt.h"

//Declare the states for the program and the corresponding variable
enum STATE {WE, WS, S};
enum STATE state = WE;

//Declare the variables across the whole program for interrupts
volatile int time_count = 3;     	//Time count value
volatile _Bool WE_light = TRUE;		//Indicate the light status of WE state 
volatile _Bool green_flag = TRUE;	//Control Red Light(FALSE) or Green Light (TRUE) across all states
volatile _Bool button_s = FALSE;  //Signal from the sensor S
volatile _Bool button_w = FALSE;	//Signal from the sensor WS

//Define the count value for SysTick from the calculation
#define SYSTICK_LVR 32768

//Macro define when there are keys pressed in each column
#define C1_pressed (!(PA->PIN & (1<<2)))	//Column 1 is LOW -> One of the keys in column 1 is pressed
#define C3_pressed (!(PA->PIN & (1<<0)))  //Column 3 is LOW -> One of the keys in column 3 is pressed


int main(void) {
    //System initialization start-------------------
    SYS_UnlockReg();	// Unlock protected registers
	
    //Enable clock sources
    CLK->PWRCON |= (1 << 0);	//12 MHz
    while(!(CLK->CLKSTATUS & (1 << 0)));  //Wait for 12MHz clock source to be stable
		CLK->PWRCON |= (1 << 1);	//32.768 KHz
		while(!(CLK->CLKSTATUS & (1 << 1)));	//Wait for 32.768kHz clock source to be stable
	
		//Select CPU clock CLKSEL0[2:0]
    CLK->CLKSEL0 &= ~(0b111 << 0);	//12 MHz
    //CPU clock frequency divider
    CLK->CLKDIV &= ~(0xF);	//Clear
    //System initialization end---------------------
    
    //System Tick initialization start--------------
    //Select STCLK as SysTick clock source
    SysTick->CTRL &= ~(1 << 2);
	
		//Systick interrupt
		SysTick->CTRL |= (1 << 1);
	
	//Select clock source for SysTick CLKSEL0[5:3]
    CLK->CLKSEL0 &= ~(0b111 << 3);
    CLK->CLKSEL0 |= (0b001 << 3);	//32.768 kHz
    
    //SysTick Reload Value
    SysTick->LOAD = SYSTICK_LVR - 1;
		//Initial Systick count value
    SysTick->VAL = 0;
		
    //Start SysTick
    SysTick->CTRL |= (1 << 0);
    //System Tick initialization end----------------
		
    SYS_LockReg();  // Lock protected registers
		
    //GPIO initialization start --------------------
		//Set output push-pull for GPC12 - GPC15 (LED5 - LED8) 
    PC->PMD &= ~(0b11 << 24);
    PC->PMD |= (0b01 << 24);	//GPC12 (LED5)
		PC->PMD &= ~(0b11 << 26);
    PC->PMD |= (0b01 << 26);	//GPC13 (LED6)
		PC->PMD &= ~(0b11 << 28);
    PC->PMD |= (0b01 << 28);	//GPC14 (LED7)
		PC->PMD &= ~(0b11 << 30);
    PC->PMD |= (0b01 << 30);	//GPC15 (LED8)
		
		//Set mode for PC4 to PC7 to control U14->U11
    PC->PMD &= (~(0xFF<< 8)); //clear PMD[15:8] 
    PC->PMD |= (0b01010101 << 8);     //Set output push-pull for PC4 to PC7
		
		//Set mode for PE0 to PE7 for segments on 7-segments
		PE->PMD &= (~(0xFFFF<< 0)); //clear PMD[15:0] 
		PE->PMD |= 0b0101010101010101<<0;   //Set output push-pull for PE0 to PE7
		
		//Turn on the digits U11, U12 and U13
		PC->DOUT &= ~(1 << 7);	//U11
		PC->DOUT &= ~(1 << 6);	//U12
		PC->DOUT &= ~(1 << 5);	//U13
		//PC->DOUT &= ~(1 << 4);

		//Configure GPIO for Key Matrix
		//Rows - outputs
		PA->PMD &= (~(0b11<< 6));
    PA->PMD |= (0b01 << 6);    
		PA->PMD &= (~(0b11<< 8));
    PA->PMD |= (0b01 << 8);  
		PA->PMD &= (~(0b11<< 10));
    PA->PMD |= (0b01 << 10);  
		
		//We will keep the Column as default(after reset) in quasi-mode instead of inputs
	  PA->PMD |= (0b11<< 0);
	  PA->PMD |= (0b11<< 2);
	  PA->PMD |= (0b11<< 4);
		
		//GPIO initialization end ----------------------
		
	while(1){

	switch (state) {
			//Base case
			case WE:
				//When 3 seconds have passed from green light, check for the signal from button_w and button_s
				if (time_count < 1) { 
					green_flag = !green_flag;	//Alter between Red Light and Green Light
					
					//If there is signal from button_w during Green Light, wait for Red Light then go to state WS (West South)
					if ((button_w) && (!green_flag)) {
						state = WS;
						green_flag = TRUE;
					}
					
					//If there is signal from button_s and no signal from button_w during Green Light, wait for Red Light then go to state S (South)
					if ((button_s) && (!green_flag)) {
						green_flag = TRUE;
						
						//If it not pressed the same time with button_w during Green Light
						if (!button_w) {
							state = S;
						}
					}
					
					time_count = 3;	//Reset count time for WE state
				}			
				
				//If light is green for WE state
				if (green_flag) {
					//Turn on led_w and led_s, turn off led_ws and led_s
					PC->DOUT &= ~(1 << 12);
					PC->DOUT &= ~(1 << 13); 
					PC->DOUT |= (1 << 14);	
					PC->DOUT |= (1 << 15);	
					
					WE_light = TRUE;	//Update status of light (Green light = TRUE)
				}	
				else {
					//Turn off all led
					PC->DOUT |= (1 << 12);
					PC->DOUT |= (1 << 13);
					PC->DOUT |= (1 << 14);
					PC->DOUT |= (1 << 15);
					
					WE_light = FALSE;	//Update status of light (Red light = FALSE)
				}				
				//Choose the correspond 7-segment for the state and display the time count on the LED
				state_7seg(state);	
				display_LED(time_count);				
				break;
			
			//When have signal from button_w
			case WS:
				time_count = 2;	//Set count time to 2s for WS state
			
				resetSysTickValue(); //Reset SysTick value to make sure it didn't continue counting from the last value of state WE
				
				while (button_w){
					//After 2 second, no green light for WS state
					if (time_count < 1) {
						green_flag = FALSE;
					}
					
					//If light is green for WS state
					if (green_flag) {
						//Turn on led_ws, turn off led_ws, led_e and led_s
						PC->DOUT |= (1 << 12);
						PC->DOUT |= (1 << 13); 
						PC->DOUT &= ~(1 << 14);
						PC->DOUT |= (1 << 15);
					}
					//If the 1st button (K1) in column 1 is pressed and WE_light is red
					else if (button_s && !green_flag) {
							green_flag = TRUE;
							state = S; 	
							button_w = FALSE;
					}
					//If the 1st button (K1) in column 1 is NOT pressed then go back to Green Light of state WE
					else {
						state = WE;
						green_flag = TRUE;
						time_count = 3;
						button_w = FALSE;
					}
					//Choose the correspond 7-segment for the state and display the time count on the LED
					state_7seg(state);
					display_LED(time_count);
					
					//Check if button_s is pressed
					PA->DOUT &= ~(1<<3);
					PA->DOUT &= ~(1<<4);
					PA->DOUT &= ~(1<<5);
					
					//If button from 3rd column is pressed
					if (C3_pressed) {
						//Check which button in column 1 is pressed
						search_col3();
					}
				}
				break;
			
			//When have signal from button_s
			case S:		
				time_count = 2;	//Set count time to 2s for S state
			
				while (button_s) {
					//After 2 second, no green light for S state
					if (time_count < 1) { 
						green_flag = FALSE;
					}
				
					//If light is green for S state
					if (green_flag) {
						//Turn on led_s, turn off led_ws, led_e and led_w
						PC->DOUT |= (1 << 12);
						PC->DOUT |= (1 << 13); 
						PC->DOUT |= (1 << 14);
						PC->DOUT &= ~(1 << 15);
					}
					//Go to WE state (Green Light) if no more green light
					else {
						state = WE;
						green_flag = TRUE;
						time_count = 3;
						button_s = FALSE;
					}
					//Choose the correspond 7-segment for the state and display the time count on the LED
					state_7seg(state);
					display_LED(time_count);
				}
				break;
			}
				
				//Make all the ROWS LOW to check for button pressing from the keypad
				PA->DOUT &= ~(1<<3);
				PA->DOUT &= ~(1<<4);
				PA->DOUT &= ~(1<<5);
				
			//If button from 1st column is pressed
				if (C1_pressed) {
					//Check which button in column is pressed
					search_col1();
					
					//If the 3rd button in column 3 is pressed and WE_light is red
					if (button_w && WE_light == FALSE) {
							green_flag = TRUE;
							state = WS;
					}
				}
				
				//If button from 3rd column is pressed
				if (C3_pressed) {
					//Check which button in column 1 is pressed
					search_col3();
					
					//If the 1st button (K1) in column is pressed and WE_light is red
					if (button_s && WE_light == FALSE) {
							green_flag = TRUE;
							
						//If it not pressed the same time with button_w
							if (!button_w) {
								state = S; 	
							}
					}
				}
				
	}
}

//------------------------------------------- main.c CODE ENDS ---------------------------------------------------------------------------

//Function to reset the value of SysTick when transition to a new state
void resetSysTickValue () {
	SYS_UnlockReg();
	
   //Select STCLK as SysTick clock source
    SysTick->CTRL &= ~(1 << 2);
	
		//Systick interrupt
		SysTick->CTRL |= (1 << 1);
	
		//Select clock source CLKSEL0[5:3]
    CLK->CLKSEL0 &= ~(0b111 << 3);
    CLK->CLKSEL0 |= (0b001 << 3);	//32.768 kHz
    
    //SysTick Reload Value
    SysTick->LOAD = SYSTICK_LVR - 1;
		//Initial Systick count value
    SysTick->VAL = 0;
		
    //Start SysTick
    SysTick->CTRL |= (1 << 0);
	
	SYS_LockReg();
}

//ISR of the SysTick to handle the time of the program, go down by 1 every 1 second passed
void SysTick_Handler(void) {
		time_count--;
}

//Function to display the number for the corresponding value on the 7-segment
void display_LED(int num) {
		switch (num)
  {
    case 0: // Number 0
      PE->DOUT = 0b10000010;
      break;
    case 1: // Number 1
      PE->DOUT = 0b11101110;
      break;
    case 2: // Number 2
      PE->DOUT = 0b00000111;
      break;
    case 3: // Number 3
      PE->DOUT = 0b01000110;
      break;
    case 4: // Number 4
      PE->DOUT = 0b01101010;
      break;
    case 5: // Number 5
      PE->DOUT = 0b01010010;
      break;
    case 6: // Number 6
      PE->DOUT = 0b00010010;
      break;
    case 7: // Number 7
      PE->DOUT = 0b11100110;
      break;
    case 8: // Number 8
      PE->DOUT = 0b00000010;
      break;
    case 9: // Number 9
			PE->DOUT = 0b01000010;
      break;
  }
}

//Function to choose the corresponding 7-segment LED regarding each state
void state_7seg(char status) {
		switch (status) {
			case WE:
				//Turn on the U11, the rest are off in state WE (West East)
				PC->DOUT |= (1 << 7);
				PC->DOUT &= ~(1 << 6);
				PC->DOUT &= ~(1 << 5);
				PC->DOUT &= ~(1 << 4);
				break;
			case WS:
				//Turn on the U12, the rest are off in state WS (West South)
				PC->DOUT &= ~(1 << 7);
				PC->DOUT |= (1 << 6);
				PC->DOUT &= ~(1 << 5);
				PC->DOUT &= ~(1 << 4);
				break;
			case S:
				//Turn on the U13, the rest are off in state S (South)
				PC->DOUT &= ~(1 << 7);
				PC->DOUT &= ~(1 << 6);
				PC->DOUT |= (1 << 5);
				PC->DOUT &= ~(1 << 4);
				break;
		}
}

//Since only the K1 and K9 key are included in this program, only column 1 and column 3 will be taken into account and neglect the column2
//Function to search for the K1 key at column 1
void search_col1(void) {
  // Drive ROW1 output pin as LOW. Other ROW pins as HIGH
	PA->DOUT &= ~(1<<3);
	PA->DOUT |= (1<<4);
	PA->DOUT |= (1<<5);
	
	//If the 1st button (K1) is detected
  if (C1_pressed) {
		//Indicate the button_w signal was pressed
		button_w = TRUE;
    return;
  }
  else {
		// Drive ROW2 output pin as LOW. Other ROW pins as HIGH
		PA->DOUT |= (1<<3);
		PA->DOUT &= ~(1<<4);
		PA->DOUT |= (1<<5);
		if (C1_pressed)
		{
					// If column1 is LOW, detect key press as K4 (KEY 4)
					button_w = FALSE;
			return;
		}
		else	{
				// Drive ROW3 output pin as LOW. Other ROW pins as HIGH
			PA->DOUT |= (1<<3);
			PA->DOUT |= (1<<4);
			PA->DOUT &= ~(1<<5);
			if (C1_pressed)
			{
						// If column1 is LOW, detect key press as K7 (KEY 7)
						button_w = FALSE;
				return;
			}
		}
	}
}


//Function to search for the K9 key at column 3
void search_col3(void)
{
    // Drive ROW1 output pin as LOW. Other ROW pins as HIGH
	PA->DOUT &= ~(1<<3);
	PA->DOUT |= (1<<4);
	PA->DOUT |= (1<<5);
			 
	if (C3_pressed)
	{
			// If column3 is LOW, detect key press as K3 (KEY 3)
			return;
	}
	else
	{
			// Drive ROW2 output pin as LOW. Other ROW pins as HIGH
		PA->DOUT |= (1<<3);
		PA->DOUT &= ~(1<<4);
		PA->DOUT |= (1<<5);
		if (C3_pressed)
		{
				// If column3 is LOW, detect key press as K6 (KEY 6)
				return;
		}
		else
		{
				// Drive ROW3 output pin as LOW. Other ROW pins as HIGH
			PA->DOUT |= (1<<3);
			PA->DOUT |= (1<<4);
			PA->DOUT &= ~(1<<5);
			if (C3_pressed)
			{
					// If column3 is LOW, detect key press as K9 (KEY 9)
					button_s = TRUE;	//Indicate the button_s signal was pressed
					return;
			}
		}
	}
}

