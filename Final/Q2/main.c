//------------------------------------------- main.c CODE STARTS ---------------------------------------------------------------------------
#include <stdio.h>
#include <stdbool.h>
#include "NUC100Series.h"
#include "LCD.h"
#include "MCU_init.h"
#include "SYS_init.h"

void System_Config(void);
void UART0_Config(void);
void UART0_SendChar(char ch);
void UART02_IRQHandler(void);
void LCD_start(void);
void LCD_command(unsigned char temp);
void LCD_data(unsigned char temp);
void LCD_clear(void);
void LCD_SetAddress(uint8_t PageAddr, uint8_t ColumnAddr);
void SPI3_Config(void);

char ReceivedByte[132];            // This raw array store all the package from thje STM32
char Latitude[9], Longtitude[10];  // These 2 arrays store the longtitude and latitude taken from the raw array
int count = 0,                     // Count value for the index of raw array
    count_lat = 0, count_long = 0; // Count value for the index of Latitude and Longtitude arrays
volatile char byte;                // Temporary variable to hold the recevied byte
volatile bool full = false, new_data = false;

int main(void) {
    System_Config();
    UART0_Config();
    SPI3_Config();
    LCD_start();
    LCD_clear();

    // Reset all the arrays before storing the data from STM32
    memset(ReceivedByte, '0', sizeof(ReceivedByte));
    memset(Latitude, '0', sizeof(Latitude));
    memset(Longtitude, '0', sizeof(Longtitude));

    while (1) {
        // If there is new byte coming and the data isn't full yet
        if ((new_data) && (full == false)) {

            ReceivedByte[count] = byte; // Assign the current byte to the correspond index of the raw array
            new_data = false;           // Indicate the data is stored
            count++;                    // Increase by one to go to the next index of the raw array
        }

        // If the recevied data is full, only then display the data on the LCD screen
        if (full) {
            // Extract the raw data to have the Latitude
            for (int i = 21; i < 29; i++) {
                Latitude[count_lat] = ReceivedByte[i];
                count_lat++;
            }
            // Extract the raw data to have the Longtitude
            for (int j = 30; j < 39; j++) {
                Longtitude[count_long] = ReceivedByte[j];
                count_long++;
            }

            // Display te info
            LCD_clear();
            CLK_SysTickDelay(100);
            printS_5x7(0, 10, "Latitude: ");
            printS_5x7(75, 10, Latitude);
            printS_5x7(0, 48, "Longtitude: ");
            printS_5x7(75, 48, Longtitude);
            full = false; // Indicate all the data is received and extracted, go on to the new package
            // Reset the count values for the index of the arrays
            count = 0;
            count_lat = 0;
            count_long = 0;
        }
    }
}

void System_Config(void) {
    SYS_UnlockReg(); // Unlock protected registers

    // enable clock sources
    CLK->PWRCON |= (1 << 0);
    while (!(CLK->CLKSTATUS & (1 << 0)));
    CLK->PWRCON |= (1 << 2);
    while (!(CLK->CLKSTATUS & (1 << 4)));
    CLK->PWRCON |= (1 << 3);
    while (!(CLK->CLKSTATUS & (1 << 3)));

    // PLL configuration starts
    CLK->PLLCON &= ~(1 << 19); // 0: PLL input is HXT
    CLK->PLLCON &= ~(1 << 16); // PLL in normal mode
    CLK->PLLCON &= (~(0x01FF << 0));
    CLK->PLLCON |= 48;
    CLK->PLLCON &= ~(1 << 18); // 0: enable PLLOUT
    while (!(CLK->CLKSTATUS & (0x01 << 2)));
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

    SYS_LockReg(); // Lock protected registers

    // Configuring the ISR service for UART0
    NVIC->ISER[0] |= (1 << 12);
    NVIC->IP[3] &= ~(0b11 < 6);
}

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
    UART0->BAUD |= 70;
    UART0->BAUD &= ~(0x03 << 28); // mode 0
}

void UART0_SendChar(char ch) {
    while (UART0->FSR & (1UL << 23)); // wait until TX FIFO is not full
    UART0->THR = ch;
}

//------------------------------------------- main.c CODE ENDS ---------------------------------------------------------------------------

// This IRQ handle the data coming from the UART0
void UART02_IRQHandler(void) {
    // If there is a byte coming, assign it to the variable so it can be stored
    byte = UART0->RBR;
    new_data = true; // Indicate there's a new byte from the interrupt

    // If the last byte is the carriage return, it means we are at the end of the package so indicate that the received data is full
    if (byte == '\r') {
        full = true;
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
    while (SPI3->CNTRL & (1 << 0));
    SPI3->SSR &= ~(1 << 0);
}

void LCD_data(unsigned char temp) {
    SPI3->SSR |= 1 << 0;
    SPI3->TX[0] = 0x0100 + temp;
    SPI3->CNTRL |= 1 << 0;
    while (SPI3->CNTRL & (1 << 0));
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