//------------------------------------------- main.c CODE STARTS ---------------------------------------------------------------------------
#include <stdio.h>
#include "NUC100Series.h"
#include "LCD.h"
#include "MCU_init.h"
#include "SYS_init.h"

#define TIMER0_COUNTS 500 - 1

void System_Config(void);
void UART0_Config(void);
void UART0_SendChar(int ch);
char UART0_GetChar(void);
void get_map(void);
void LCD_start(void);
void LCD_command(unsigned char temp);
void LCD_data(unsigned char temp);
void LCD_clear(void);
void LCD_SetAddress(uint8_t PageAddr, uint8_t ColumnAddr);
void SPI3_Config(void);
void display_LED(int num);
void KeyPadEnable(void);
uint8_t KeyPadScanning(void);
void display_info(void);
void Buzzer_beep(void);

void UART02_IRQHandler(void);
void EINT1_IRQHandler(void);
void TMR0_IRQHandler(void);
void GPAB_IRQHandler(void);

volatile char ReceivedByte1[128]; // This array store the raw data from the sent map via the laptop
volatile char map[8][8]           // This 2-D array store the actual map that will be used later in the program
    ,
    hit_map[8][8]; // This 2-D array store the location that was hit in the program

// Count values to be used in the program
volatile int count = 0, shot_weight10 = 0, shot_weight1 = 0, shots = 0, x_axis = 1, y_axis = 1, hit_shot = 0, seven_segment = 0;

// Flags to indicate status in the program
volatile _Bool map_loading = FALSE, map_loaded = FALSE, choose_x = TRUE, buzzer_flag = TRUE, miss = FALSE, display = FALSE, reset_turn = FALSE;

// Four states in the program
enum STATE { welcom,
             load_map,
             game_start,
             shoot,
             game_over };
enum STATE state = welcom; // Starting state

int main(void) {
    uint8_t pressed_key = 0; // This variable holds the value from the keypad

    // Start configuring the whole system
    System_Config();
    UART0_Config();
    SPI3_Config();
    LCD_start();
    LCD_clear();

    while (1) {
        // Go through between states

        // In state welcom
        if (state == welcom) {
            // Display the welcom message
            LCD_clear();
            printS_5x7(30, 0, "RMIT University ");
            printS_5x7(45, 20, "EEET2481");
            printS_5x7(10, 35, "Embedded System Design");
            printS_5x7(15, 50, "Welcom to the game!");
            CLK_SysTickDelay(10000);

            memset(hit_map, '0', sizeof(hit_map[0][0]) * 8 * 8); // Reset the hit_map

            buzzer_flag = TRUE; // setbuf the flag for game over state

            // Wait until map is loaded then go to
            if (map_loading)
                state = load_map;
        }

        // In state load map
        else if (state == load_map) {
            reset_turn = FALSE; // Indicate the game has restarted again and another map is gonna be loaded

            // If the map is done loading then display the message
            if (map_loaded) {
                CLK_SysTickDelay(10000);
                LCD_clear();
                printS_5x7(8, 30, "Map loaded sucessfully!");

            } else {
                memset(map, '0', sizeof(map[0][0]) * 8 * 8); // Reset the hit_map

                // If not done then display map is still loading and extract the map from the data
                CLK_SysTickDelay(10000);
                LCD_clear();
                printS_5x7(30, 30, "Map is loading");

                // Extract the map from received bytes
                get_map();

                // Send the whole map to the laptop again to confirm the result
                for (int k = 0; k < 8; k++) {
                    for (int i = 0; i < 8; i++) {
                        UART0_SendChar(map[k][i]);
                        CLK_SysTickDelay(20000);
                    }
                    UART0_SendChar(' '); // White space to separate the linesca
                }

                // Map is done loaded, ready to start the game
                map_loaded = TRUE;
            }
        }

        // While the game is started
        else if (state == game_start) {
            // Display the information using the 4 digits
            display = TRUE;
            CLK_SysTickDelay(10000);
            LCD_clear();

            // Loop through the hit map and display the 'x' mark on where it was previously hit (indicated by '1')
            for (int i = 0; i < 8; i++) {
                for (int j = 0; j < 8; j++) {
                    if (hit_map[i][j] == '1') {
                        printC_5x7(j * 16 + 6, i * 8, 'x');

                    } else {
                        printC_5x7(j * 16 + 6, i * 8, '-');
                    }
                }
            }

            // Scanning the value of the keypad
            pressed_key = KeyPadScanning();

            // If any key on keypad is pressed
            if (pressed_key != 0) {
                // If the keypad is K9 then alter between choose x-axis and y-axis
                if (pressed_key == 9) {
                    choose_x = !choose_x;
                } else {
                    // If choose_x is TRUE -> choose x axis.
                    //	  choose_x if FALSE -> choose y axis.
                    if (choose_x) {
                        x_axis = pressed_key;
                    } else {
                        y_axis = pressed_key;
                    }
                }
            }

            // If choosing x axis -> Turn on LED7 to indicate
            if (choose_x) {
                PC->DOUT &= ~(1 << 14);
                PC->DOUT |= (1 << 15);
            }

            // If choosing y axis -> Turn on LED8 to indicte
            else {
                PC->DOUT |= (1 << 14);
                PC->DOUT &= ~(1 << 15);
            }

            // If the number of hit is 10 or the number of shots exceed 16 then
            // ends the game, go to game_over state, reset the values
            if ((hit_shot == 10) || (shots > 15)) {
                hit_shot = 0;
                shots = 0;
                shot_weight1 = 0;
                shot_weight10 = 0;
                x_axis = 1;
                y_axis = 1;

                state = game_over;
            }

            // Advoid the debounce of keypad
            CLK_SysTickDelay(100000);
        }

        // Enter shoot mode
        else if (state == shoot) {
            // Turn off LED7 and LED8 while in shoot mode
            PC->DOUT |= (1 << 14);
            PC->DOUT |= (1 << 15);

            // Calculate the weight of the shots to display on U13 and U14
            shots++; // Increase the shot value by one
            shot_weight1 = shots % 10;
            shot_weight10 = (shots - shot_weight1) / 10;

            // Check if the chosen location is hit once, hit more than twice or miss

            // Check if the location is part of the ship of the map.
            if (map[x_axis - 1][y_axis - 1] == '1') {
                // If previously wasn't hit then inidicate the location is hit
                // and record the location to the hit map, increase the hit shot by one
                if (hit_map[x_axis - 1][y_axis - 1] != '1') {
                    LCD_clear();
                    printS(50, 20, "Hit!");
                    hit_map[x_axis - 1][y_axis - 1] = '1';
                    hit_shot++;

                    // Flash the LED5 3 times to indicate in the hit
                    for (int i = 0; i < 7; i++) {
                        PC->DOUT ^= (1 << 12);
                        CLK_SysTickDelay(1000000);
                    }
                    PC->DOUT |= (1 << 12);
                }
            }
            // If miss the ship
            else {
                LCD_clear();
                printS(40, 20, "Missed!");
                CLK_SysTickDelay(2000000);
            }

            // Back to the game play
            state = game_start;

        }

        // When game is over
        else if (state == game_over) {
            // Display Game over to the user and buzzer beeps 5 times only once
            LCD_clear();
            printS(27, 22, "Game over");

            CLK_SysTickDelay(10000);
            if (buzzer_flag) {
                Buzzer_beep();
            }
            buzzer_flag = FALSE;

            // Turn off all digits and LEDs
            PC->DOUT |= (1 << 12);
            PC->DOUT |= (1 << 13);
            PC->DOUT |= (1 << 14);
            PC->DOUT |= (1 << 15);
            PC->DOUT &= ~(1 << 7);
            PC->DOUT &= ~(1 << 6);
            PC->DOUT &= ~(1 << 5);
            PC->DOUT &= ~(1 << 4);

            count = 0;
        }
    }
}

// This function is used  to confige the clock sources, PLL, outputs, timer and interrupts
void System_Config(void) {
    SYS_UnlockReg(); // Unlock protected registers

    // enable clock sources
    CLK->PWRCON |= (1 << 0);
    while (!(CLK->CLKSTATUS & (1 << 0)))
        ;
    CLK->PWRCON |= (1 << 2);
    while (!(CLK->CLKSTATUS & (1 << 4)))
        ;
    CLK->PWRCON |= (1 << 3);
    while (!(CLK->CLKSTATUS & (1 << 3)))
        ;

    // PLL configuration starts
    CLK->PLLCON &= ~(1 << 19); // 0: PLL input is HXT
    CLK->PLLCON &= ~(1 << 16); // PLL in normal mode
    CLK->PLLCON &= (~(0x01FF << 0));
    CLK->PLLCON |= 48;
    CLK->PLLCON &= ~(1 << 18); // 0: enable PLLOUT
    while (!(CLK->CLKSTATUS & (0x01 << 2)))
        ;
    // PLL configuration ends

    // CPU clock source selection
    CLK->CLKSEL0 &= (~(0x07 << 0));
    CLK->CLKSEL0 |= (0x02 << 0);
    // clock frequency division
    CLK->CLKDIV &= (~0x0F << 0);

    // UART0 Clock selection and configuration
    CLK->CLKSEL1 |= (0x03 << 24); // UART0 clock source is 22.1184 MHz
    CLK->CLKDIV &= ~(0x0F << 8);  // clock divider is 1
    CLK->APBCLK |= (0x01 << 16);  // enable UART0 clock
    CLK->APBCLK |= 1 << 15;       // Enable clock for SPI3

    PC->PMD &= ~(0b11 << 24);
    PC->PMD |= (0b01 << 24);
    PC->PMD &= ~(0b11 << 26);
    PC->PMD |= (0b01 << 26);
    PC->PMD &= ~(0b11 << 28);
    PC->PMD |= (0b01 << 28);
    PC->PMD &= ~(0b11 << 30);
    PC->PMD |= (0b01 << 30);
    PC->PMD &= ~(0b11 << 22);
    PC->PMD |= (0b01 << 22);
    // Set mode for PC4 to PC7
    PC->PMD &= (~(0xFF << 8));    // clear PMD[15:8]
    PC->PMD |= (0b01010101 << 8); // Set output push-pull for PC4 to PC7
    // Set mode for PE0 to PE7
    PE->PMD &= (~(0xFFFF << 0));        // clear PMD[15:0]
    PE->PMD |= 0b0101010101010101 << 0; // Set output push-pull for PE0 to PE7

    // Turn on the digits U11, U12, U13 and U14
    PC->DOUT &= ~(1 << 7);
    PC->DOUT &= ~(1 << 6);
    PC->DOUT &= ~(1 << 5);
    PC->DOUT &= ~(1 << 4);

    // Timer 0 initialization start--------------
    // TM0 Clock selection and configuration
    CLK->CLKSEL1 &= ~(0b111 << 8);

    CLK->APBCLK |= (1 << 2);
    // Pre-scale =
    TIMER0->TCSR &= ~(0xFF << 0);
    TIMER0->TCSR |= 11 << 0;
    // reset Timer 0
    TIMER0->TCSR |= (1 << 26);
    // define Timer 0 operation mode
    TIMER0->TCSR &= ~(0b11 << 27);
    TIMER0->TCSR |= (0b01 << 27);
    TIMER0->TCSR &= ~(1 << 24);
    // TDR to be updated continuously while timer counter is counting
    TIMER0->TCSR |= (1 << 16);
    // Enable TE bit (bit 29) of TCSR
    // The bit will enable the timer interrupt flag TIF
    TIMER0->TCSR |= (1 << 29);
    // TimeOut = 0.01s --> Counter's TCMPR = 1000-1
    TIMER0->TCMPR = TIMER0_COUNTS;
    // start counting
    TIMER0->TCSR |= (1 << 30);

    SYS_LockReg(); // Lock protected registers

    // Setting up interrupt and debounce for GPB15 button

    // GPB15 is output
    PB->PMD &= ~(0b11 << 30);
    PB->IMD &= ~(1 << 15); // Edge trigger interrupt
    PB->IEN |= (1 << 15);

    PB->DBEN |= (1 << 15);
    GPIO->DBNCECON |= (1 << 4); // De-bounce counter clock source is the internal 10 kHz
    GPIO->DBNCECON |= (8 << 0); // Sample interrupt input once per 256 clocks
    NVIC->ISER[0] |= (1 << 3);
    NVIC->IP[0] &= ~(0b11 << 30);

    // Configuring the ISR service for UART0
    NVIC->ISER[0] |= (1 << 12);
    NVIC->IP[3] &= ~(0b11 < 6);

    // Configuring the ISR service for Timer0
    NVIC->ISER[0] |= (1 << 8);
    NVIC->IP[2] &= ~(0b11 << 6);
}

// This function config the settings for the UART0 to receive the data for the map from the laptop
void UART0_Config(void) {
    // UART0 pin configuration. PB.1 pin is for UART0 TX
    PB->PMD &= ~(0b11 << 2);
    PB->PMD |= (0b01 << 2);   // PB.1 is output pin
    SYS->GPB_MFP |= (1 << 1); // GPB_MFP[1] = 1 -> PB.1 is UART0 TX pin
    SYS->GPB_MFP |= (1 << 0); // GPB_MFP[0] = 1 -> PB.0 is UART0 RX pin
    PB->PMD &= ~(0b11 << 0);  // Set Pin Mode for GPB.0(RX - Input)

    // UART0 operation configuration
    UART0->FCR |= (0x03 << 1);   // clear both TX & RX FIFO
    UART0->FCR &= ~(0x0F << 16); // FIFO Trigger Level is 1 byte
    UART0->FCR &= ~(0xF << 4);   // FIFO Trigger Level is 1 byte
    UART0->LCR &= ~(0x01 << 3);  // no parity bit
    UART0->LCR &= ~(0x01 << 2);  // one stop bit
    UART0->LCR |= (0x03 << 0);   // 8 data bit
    UART0->IER |= (1 << 0);      // Enable the interrupt bit for UART0

    // Baud rate config: BRD/A = 1, DIV_X_EN=0
    // --> Mode 0, Baud rate = UART_CLK/[16*(A+2)] = 22.1184 MHz/[16*(1+2)]= 460800 bps
    UART0->BAUD &= ~(0x0FFFF << 0);
    UART0->BAUD |= 10;
    UART0->BAUD &= ~(0x03 << 28); // mode 0
}

// This function send the data back to laptop for approving the correctness
void UART0_SendChar(int ch) {
    while (UART0->FSR & (0x01 << 23))
        ; // wait until TX FIFO is not full
    UART0->DATA = ch;
    if (ch == '\n') {
        while (UART0->FSR & (0x01 << 23))
            ;
        UART0->DATA = '\r';
    }
}

//------------------------------------------- main.c CODE ENDS ---------------------------------------------------------------------------
void get_map(void) {
    // On the first line, the first character to extract from is at index number 0
    // On the second line and after, the first character to extract from is at the index number 17th, 34th, ..., 119th
    // We scan through the raw array and take every other index because on each line there will be a  white space separate each character
    for (int row = 0; row < 8; row++) {
        for (int col = 0; col < 8; col++) {
            map[row][col] = ReceivedByte1[col * 2 + row * 17];
        }
    }
}

// This IRQ is for the UART0 that handle the incoming info from the latop
void UART02_IRQHandler(void) {
    map_loading = TRUE; // Indicate map is still loading
    if (UART0->ISR & (1 << 0)) {
        // Store the data to the current location of the raw araray
        ReceivedByte1[count] = UART0->RBR;

        count++; // Move to next index of the array to store info
    }
    // If count exceed 131 bytes then the procecss is completed
    if (count >= 129) {
        map_loading = FALSE;
    }
}

void LCD_start(void) {
    LCD_command(0xE2);
    LCD_command(0xA1);
    LCD_command(0xEB);

    LCD_command(0x81);
    LCD_command(0xA0);
    LCD_command(0xC0);
    LCD_command(0xAF);
}

void LCD_command(unsigned char temp) {
    SPI3->SSR |= 1 << 0;
    SPI3->TX[0] = temp;
    SPI3->CNTRL |= 1 << 0;
    while (SPI3->CNTRL & (1 << 0))
        ;
    SPI3->SSR &= ~(1 << 0);
}

void LCD_data(unsigned char temp) {
    SPI3->SSR |= 1 << 0;
    SPI3->TX[0] = 0x0100 + temp;
    SPI3->CNTRL |= 1 << 0;
    while (SPI3->CNTRL & (1 << 0))
        ;
    SPI3->SSR &= ~(1 << 0);
}

void LCD_clear(void) {
    int16_t i;
    LCD_SetAddress(0x0, 0x0);
    for (i = 0; i < 132 * 8; i++) {
        LCD_data(0x00);
    }
}

void LCD_SetAddress(uint8_t PageAddr, uint8_t ColumnAddr) {
    LCD_command(0xB0 | PageAddr);
    LCD_command(0x10 | (ColumnAddr >> 4) & 0xF);
    LCD_command(0x00 | (ColumnAddr & 0xF));
}

void SPI3_Config(void) {
    SYS->GPD_MFP |= 1 << 11; // 1: PD11 is configured for alternative function
    SYS->GPD_MFP |= 1 << 9;  // 1: PD9 is configured for alternative function
    SYS->GPD_MFP |= 1 << 8;  // 1: PD8 is configured for alternative function

    SPI3->CNTRL &= ~(1 << 23); // 0: disable variable clock feature
    SPI3->CNTRL &= ~(1 << 22); // 0: disable two bits transfer mode
    SPI3->CNTRL &= ~(1 << 18); // 0: select Master mode
    SPI3->CNTRL &= ~(1 << 17); // 0: disable SPI interrupt
    SPI3->CNTRL |= 1 << 11;    // 1: SPI clock idle high
    SPI3->CNTRL &= ~(1 << 10); // 0: MSB is sent first
    SPI3->CNTRL &= ~(3 << 8);  // 00: one transmit/receive word will be executed in one data transfer

    SPI3->CNTRL &= ~(31 << 3); // Transmit/Receive bit length
    SPI3->CNTRL |= 9 << 3;     // 9: 9 bits transmitted/received per data transfer

    SPI3->CNTRL |= (1 << 2); // 1: Transmit at negative edge of SPI CLK
    SPI3->DIVIDER = 0;       // SPI clock divider. SPI clock = HCLK / ((DIVIDER+1)*2). HCLK = 50 MHz
}

void KeyPadEnable(void) {
    GPIO_SetMode(PA, BIT0, GPIO_MODE_QUASI);
    GPIO_SetMode(PA, BIT1, GPIO_MODE_QUASI);
    GPIO_SetMode(PA, BIT2, GPIO_MODE_QUASI);
    GPIO_SetMode(PA, BIT3, GPIO_MODE_QUASI);
    GPIO_SetMode(PA, BIT4, GPIO_MODE_QUASI);
    GPIO_SetMode(PA, BIT5, GPIO_MODE_QUASI);
}

// This function return the number from the keypad that user presses
uint8_t KeyPadScanning(void) {
    PA0 = 1;
    PA1 = 1;
    PA2 = 0;
    PA3 = 1;
    PA4 = 1;
    PA5 = 1;
    if (PA3 == 0)
        return 1;
    if (PA4 == 0)
        return 4;
    if (PA5 == 0)
        return 7;
    PA0 = 1;
    PA1 = 0;
    PA2 = 1;
    PA3 = 1;
    PA4 = 1;
    PA5 = 1;
    if (PA3 == 0)
        return 2;
    if (PA4 == 0)
        return 5;
    if (PA5 == 0)
        return 8;
    PA0 = 0;
    PA1 = 1;
    PA2 = 1;
    PA3 = 1;
    PA4 = 1;
    PA5 = 1;
    if (PA3 == 0)
        return 3;
    if (PA4 == 0)
        return 6;
    if (PA5 == 0)
        return 9;
    return 0;
}

// GPB15 External Interrupt to handle events
void EINT1_IRQHandler(void) {
    PB->ISRC |= (1 << 15); // Clear the interrupt flag for PB15

    // If the user replay the game, system will skip the load map state to keep the old map data
    if (state == welcom && reset_turn == TRUE) {
        state = game_start;
    }

    // In game start user choose the location then press PB15 to fire
    else if (state == game_start) {
        state = shoot;
        return;
    }

    // If the map is done loaded, pressing PB15 will take the user to start the game
    else if (map_loaded && state == load_map) {
        state = game_start;
        return;
    }

    // If game is over and the buzzer done buzzing 5 times, the user can press PB15 to return to the game
    else if ((buzzer_flag == FALSE) && (state == game_over)) {
        // Reset the flags
        display = FALSE;
        map_loading = FALSE;
        map_loaded = FALSE;
        reset_turn = TRUE;
        state = welcom;
    }
}

// Timer0 Interrupt to handle the scanning rate for displaying the 4 digits (extra U12 digit for the y-axis selection)
void TMR0_IRQHandler(void) {
    // This flag indiciates when we need to display the digits
    // Everytime the timer interrupt is called it increased the seven_segement by one
    if (display) {
        seven_segment++;
        // If it reaches 4, reset to 0
        if (seven_segment == 4) {
            seven_segment = 0;
        }

        // number0: display U11 - y axis
        // number1: display U12 - x axis
        // number2: display U13 - shot weight 10
        // number3: display U14 - shot weight 1
        switch (seven_segment) {
        case 0:
            // U11 digit on, the rest are off
            PC->DOUT |= (1 << 7);
            PC->DOUT &= ~(1 << 6);
            PC->DOUT &= ~(1 << 5);
            PC->DOUT &= ~(1 << 4);
            display_LED(x_axis);
            break;

        case 1:
            // U12 digit on, the rest are off
            PC->DOUT &= ~(1 << 7);
            PC->DOUT |= (1 << 6);
            PC->DOUT &= ~(1 << 5);
            PC->DOUT &= ~(1 << 4);
            display_LED(y_axis);
            break;

        case 2:
            // U13 digit on, the rest are off
            PC->DOUT &= ~(1 << 7);
            PC->DOUT &= ~(1 << 6);
            PC->DOUT |= (1 << 5);
            PC->DOUT &= ~(1 << 4);
            display_LED(shot_weight10);
            break;

        case 3:
            // U14 digit on, the rest are off
            PC->DOUT &= ~(1 << 7);
            PC->DOUT &= ~(1 << 6);
            PC->DOUT &= ~(1 << 5);
            PC->DOUT |= (1 << 4);
            display_LED(shot_weight1);
            break;
        }
    }
    // Clear the interrupt flag for Timer0
    TIMER0->TISR |= (1 << 0);
}

// Function display the correspond number for the LEDs
void display_LED(int num) {
    switch (num) {
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

//  Function to beep the buzzer 5 times when the game is over
void Buzzer_beep(void) {
    for (int i = 0; i < (5 * 2); i++) {
        PB->DOUT ^= (1 << 11); // Buzzer is PB11
        CLK_SysTickDelay(300000);
    }

    // This flag helps the buzzer only go on once
    buzzer_flag = FALSE;
}


