// STM32L432KC_SPI.c
/*
Author: Jessica Li
Email: jesli@hmc.edu
Date: 10/17/2026
TODO: <SHORT DESCRIPTION OF WHAT THIS FILE DOES>
*/

#include "STM32L432KC_SPI.h"
#include "STM32L432KC_GPIO.h"

#include <stm32l432xx.h>

/* Enables the SPI peripheral and intializes its clock speed (baud rate), polarity, and phase.
 *    -- br: (0b000 - 0b111). The SPI clk will be the master clock / 2^(BR+1).
 *    -- cpol: clock polarity (0: inactive state is logical 0, 1: inactive state is logical 1).
 *    -- cpha: clock phase (0: data captured on leading edge of clk and changed on next edge, 
 *          1: data changed on leading edge of clk and captured on next edge)
 * Refer to the datasheet for more low-level details. */ 
void initSPI(int br, int cpol, int cpha){
    // make sure clock tree is configured before
    // turn on SPI clock domain
    RCC->APB2ENR |= (RCC_APB2ENR_SPI1EN);
    // steps to configure SPI parameters are given in the STM32 Reference Manual Section 40.4.7 Page 1313
    // 1. Configure GPIO for MOSI, MISO, and SCK pins:
    gpioEnable(GPIO_PORT_B);
    pinMode(MISO_PIN, GPIO_ALT);
    pinMode(MOSI_PIN, GPIO_ALT);
    pinMode(SCK_PIN, GPIO_ALT);
    pinMode(CS_PIN, GPIO_OUTPUT); 

    /* MISO - PB4, MOSI - PB5, SCK - PB3
    SPI = AF5 (see STM32L432KC Datasheet Table 15. Alternate function AF0 to AF7 p56)
    Register for CMSIS RM p271-2 */
    GPIOB->AFR[0] &= ~GPIO_AFRL_AFSEL4;// clear relevant fields first (uses mask)
    GPIOB->AFR[0] &= ~GPIO_AFRL_AFSEL5;// clear relevant fields first
    GPIOB->AFR[0] &= ~GPIO_AFRL_AFSEL3;// clear relevant fields first
    GPIOB->AFR[0] |= _VAL2FLD(GPIO_AFRL_AFSEL4, 0b0101); // configure GPIO pins to their alt function 5 (SPI)
    GPIOB->AFR[0] |= _VAL2FLD(GPIO_AFRL_AFSEL5, 0b0101);
    GPIOB->AFR[0] |= _VAL2FLD(GPIO_AFRL_AFSEL3, 0b0101); 
    
    // 2. Write to the SPI_CR1 register (RM p1330) --> setting SPI parameters
    SPI1->CR1 = 0; // clear first
    // 2a. Configure serial clock baud rate using BR[2:0] bits
    SPI1->CR1 |= _VAL2FLD(SPI_CR1_BR, br);
    // 2b. Configure CPOL and CPHA (clock polarity and phase) should match with slave!
    SPI1->CR1 |= _VAL2FLD(SPI_CR1_CPOL, cpol);
    SPI1->CR1 |= _VAL2FLD(SPI_CR1_CPHA, cpha);
    /* Steps 2c to 2f are already configured to 0 by the clear line, so no need
       2c. Select simplex/half-duplex mode: clear already sets to 2-line full duplex (Transmit and Receive)
       2d. Configure the LSBFIRST bit to define the frame format: set to 0, MSB-first
       2e. Configure CRCL and CRCEN bits if CRC is needed: CRCEN = 0 cuz not needed
       2f. configure SSM and SSI: Both 0, software slave management disabled, there's a separate GPIO manually toggled for sensor CE */
    // 2g. Configure the MSTR bit
    SPI1->CR1 |= _VAL2FLD(SPI_CR1_MSTR, 1); // set as master


    // 3. Write to SPI_CR2 register
    SPI1->CR2 = 0; // clear first
    // 3a. Configure the DS[3:0] bits to select the data length for the transfer (8 = DS+1)
    SPI1->CR2 |= _VAL2FLD(SPI_CR2_DS, 0b0111); 
    // 3b. Configure SSOE (SS output enable)
    SPI1->CR2 |= _VAL2FLD(SPI_CR2_SSOE, 1); // turned high for valid NSS mode... (SSM and SSI are 0)
    // 3c, 3d, and 3f stay cleared (SPI Motorola mode to match temp sensor)
    // 3e. Configure the FRXTH bit. The RXFIFO threshold must be aligned to the read access size for the SPIx_DR register.
    SPI1->CR2 |= _VAL2FLD(SPI_CR2_FRXTH, 1); // set to 1 for 8-bit frame to generate RXNE event

    // enable SPI!
    SPI1->CR1 |= _VAL2FLD(SPI_CR1_SPE, 1);
}

/* Transmits a character (1 byte) over SPI and returns the received character.
 *    -- send: the character to send over SPI
 *    -- return: the character received over SPI 
 *    (RM p1306 for general overview, general procedure on p1314 section 40.4.9)*/
char spiSendReceive(char send){
    // send (Tx shift register, transmit)
    // wait until TCE flag is set (TXFIFO has enough space to store data to send) before sending
    while(!(SPI1->SR & SPI_SR_TXE)); // while TXE bit in SR is *not* 1, wait
    *((volatile char *)(&SPI1->DR)) = send; // Write: 8-bit access, one frame

    // recieve (Rx shift register, recieve)
    // wait until RXNE flag is set (RXFIFO has received 8-bits, as set by FRXTH = 1)
    while(!(SPI1->SR & SPI_SR_RXNE)); 
    char rec = *((volatile char *)(&SPI1->DR)); // Read: 8-bit access, one frame (FRXTH = 1)

    return rec;
}