//------------------------------------------- main.c CODE STARTS ---------------------------------------------------------------------------
#include "NUC100Series.h"
#include <stdio.h>

void System_Config(void);
void enableClock(int clockFreq);
void PLLconfig(int freqIn);
void CPUconfig(int clockSrc, unsigned int clkdiv);
void SPI2_Config(void);
void ADC7_Config(void);
void ADC_IRQHandler(void);

void SPI2_TX(unsigned char temp);

volatile int adc_int_flag = 0; // Variable to indicate when the voltage is greater than 2V

int main(void) {
    // System initialization
    System_Config();
    SPI2_Config();
    ADC7_Config();

    ADC->ADCR |= (0x01ul << 11); // start ADC channel 7 conversion

    while (1) {
        // If flag is set
        if (adc_int_flag == 1) {
            NVIC->ICER[0] |= (1ul << 29); // temporarily stop conversion completion interrupt

            SPI2_TX(0x32); // Send out '2' 0b0011 0010
            CLK_SysTickDelay(2000);
            SPI2_TX(0x30); // Send out '0' 0b0011 0000
            CLK_SysTickDelay(2000);
            SPI2_TX(0x32);	//Send out '2'
            CLK_SysTickDelay(2000);
            SPI2_TX(0x32);	//Send out '2'

            adc_int_flag = 0; // Reset the flag

            CLK_SysTickDelay(200000);

            NVIC->ISER[0] |= 1ul << 29; // start conversion interrupt again. ready for the next one
        }
    }
}

// Interrupt Service Rountine of ADC
void ADC_IRQHandler(void) {
    ADC->ADSR |= (0b1 << 0); // write 1 to clear ADF when conversion is done and ISR is called

    // Get the conversion result of adc7
    uint32_t adc7_val = ADC->ADDR[7] & 0x0000FFFF;

    // If the conversion result of adc7 greater predefined-value means voltage is greater than 2V
    if (adc7_val >= 2482) { //	2/0.806 = 2181.82
        adc_int_flag = 1;
    }
}

void System_Config(void) {
    SYS_UnlockReg(); // Unlock protected registers

    // Enable and wait HXT clock source be stable
    enableClock(12);

    // PLL input is HXTt
    PLLconfig(12);

    // clock source selection
    CLK->CLKSEL0 &= (~(0x07 << 0));
    CLK->CLKSEL0 |= (0x02 << 0);

    // clock source select PLL, clock frequency division = 0
    CPUconfig(0, 0);

    // enable clock of SPI2
    CLK->APBCLK |= 1 << 14; // Enable clock for SPI2

    // ADC Clock selection and configuration
    CLK->CLKSEL1 &= ~(0b11 << 2); // ADC clock source is 12 MHz
    CLK->CLKDIV &= ~(0xFF << 16);
    CLK->CLKDIV |= (11 << 16);  // ADC clock divider is (11+1) --> ADC clock is 12/12 = 1 MHz
    CLK->APBCLK |= (0b1 << 28); // enable ADC clock

    SYS_LockReg(); // Lock protected registers
}

void enableClock(int clockFreq) {
    if (clockFreq == 12) {
        // HXT (12 MHz)
        CLK->PWRCON |= (1 << 0);
        while (!(CLK->CLKSTATUS & (1 << 0)))
            ;
    } else if (clockFreq == 32) {
        // 32.768 kHz
        CLK->PWRCON |= (1 << 1);
        while (!(CLK->CLKSTATUS & (1 << 1)))
            ;
    } else if (clockFreq == 22) {
        // 22.1184 MHz
        CLK->PWRCON |= (1 << 2);
        while (!(CLK->CLKSTATUS & (1 << 4)))
            ;
    } else if (clockFreq == 10) {
        // 10 kHz
        CLK->PWRCON |= (1 << 3);
        while (!(CLK->CLKSTATUS & (1 << 3)))
            ;
    }
}

void PLLconfig(int freqIn) {
    // PLL configuration starts
    if (freqIn == 22) {
        // 22MHz go in
        CLK->PLLCON |= (1 << 19);
    } else if (freqIn == 12) {
        // 12MHz go in
        CLK->PLLCON &= (~(1 << 19));
    }

    // Use normal mode
    CLK->PLLCON &= ~(1 << 16);

    //	F_OUT = F_IN * NF / (NR * NO) = 12 * 25 /(3 *2) = 50
    CLK->PLLCON &= (~(0xFFFF)); // Clear bit 0 to 15 for configuration
    // Set NF = FB_DV+2 [0:8]
    CLK->PLLCON |= 23; // FB_DV = 23
    // Set NR = IN_DV+2 [13:9]
    CLK->PLLCON |= (0b01 << 9); // IN_DV = 1
    // Set NO: OUT_DV = {00, 01, 10, 11} => NO = {1, 2, 3, 4} [15:14]
    CLK->PLLCON |= (0b01 << 14); // NO = 2

    // Enable PLLOUT
    CLK->PLLCON &= (~(1 << 18));

    // Wait for the PLL signal to stable
    while (!(CLK->CLKSTATUS & (1 << 2)))
        ;
    // PLL configuration ends
}

void CPUconfig(int clockSrc, unsigned int clkdiv) {
    // CLKSEL0[2:0]
    CLK->CLKSEL0 &= (~(0x07 << 0)); // CLEAR for configuration

    // PLL
    if (clockSrc == 0) {
        CLK->CLKSEL0 |= 0b010;
    }
    // 32kHz
    else if (clockSrc == 32) {
        CLK->CLKSEL0 |= 0b001;
    }
    // 12Mhz
    else if (clockSrc == 12) {
        CLK->CLKSEL0 |= 0b000;
    }
    // 22MHz
    else if (clockSrc == 22) {
        CLK->CLKSEL0 |= 0b111;
    }
    // 10kHz
    else if (clockSrc == 10) {
        CLK->CLKSEL0 |= 0b011;
    }

    // CLKDIV[3:0]
    CLK->CLKDIV &= (~(0x0F << 0));
    if (clkdiv > 0) {
        CLK->CLKDIV |= clkdiv;
    }
}

void SPI2_Config(void) {
    //--------------------------------
    // SPI2 initialization
    //--------------------------------
    SYS->GPD_MFP |= 1 << 3; // 1: PD3 is configured for alternative function (send out data)
    SYS->GPD_MFP |= 1 << 1; // 1: PD1 is configured for alternative function	(clock freq)
    SYS->GPD_MFP |= 1 << 0; // 1: PD0 is configured for alternative function	(select slave device)

    SPI2->CNTRL &= ~(1 << 23); // 0: disable variable clock feature
    SPI2->CNTRL &= ~(1 << 22); // 0: disable two bits transfer mode
    SPI2->CNTRL &= ~(1 << 18); // 0: select Master mode
    SPI2->CNTRL &= ~(1 << 17); // 0: disable SPI interrupt

    SPI2->CNTRL |= 1 << 11; // 1: SPI clock idle high

    SPI2->CNTRL |= (1 << 10); // 1: LSB is sent first

    SPI2->CNTRL &= ~(0b11 << 8); // 00: one transmit/receive word will be executed in one data transfer

    SPI2->CNTRL &= ~(0b11111 << 3);
    SPI2->CNTRL |= 8 << 3; // 8 bits/word

    SPI2->CNTRL &= ~(1 << 2); // 1: Transmit at positive edge of SPI CLK

    SPI2->DIVIDER = 24; // Divider = 24 => clock freq 1MHz SPI clock = HCLK / ((DIVIDER+1)*2
}

void ADC7_Config(void) {
    PA->PMD &= ~(0b11 << 14); // Clear
    PA->PMD |= (0b01 << 14);  // PA.7 is input pin

    PA->OFFD |= (0b1 << 7); // PA.7 digital input path is disabled

    SYS->GPA_MFP |= (0x01 << 7);   // GPA_MFP[7] = 1 for ADC7
    SYS->ALT_MFP &= ~(0x01 << 11); // ALT_MFP[11] = 0 for ADC7

    // ADC operation configuration
    ADC->ADCR |= (0x03 << 2); // continuous scan mode
    ADC->ADCR |= (0x01 << 1); // ADC interrupt is enabled

    ADC->ADCR |= (0x01 << 0); // ADC is enabled

    ADC->ADCHER &= ~(0x03 << 8); // ADC7 input source is external pin
    ADC->ADCHER |= (0x01 << 7);  // ADC channel 7 is enabled.

    // NVIC interrupt configuration for ADC interrupt source
    NVIC->ISER[0] |= 1ul << 29;
    NVIC->IP[7] &= (~(3ul << 14));
}

void SPI2_TX(unsigned char temp) {
    SPI2->SSR |= 1 << 0; // Select the slave device
    SPI2->TX[0] = temp;  // Prepare data

    SPI2->CNTRL |= 1 << 0; // start SPI data transition to PD3

    while (SPI2->CNTRL & (1 << 0))
        ; // Wait the transition complete

    SPI2->SSR &= ~(1 << 0); // Set the line to inactive state
}
//------------------------------------------- main.c CODE ENDS ---------------------------------------------------------------------------