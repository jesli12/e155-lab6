// DS1722.c
/*
Author: Jessica Li
Email: jesli@hmc.edu
Date: 10/17/2026
This is the file for temperature sensor DS1722 interface with STM32 
*/

#include "STM32L432KC_SPI.h"
#include "STM32L432KC_GPIO.h"
#include "DS1722.h"

#include <stm32l432xx.h>

void initDS1722(){
    digitalWrite(CS_PIN, PIO_HIGH);
    spiSendReceive(0x80); // write address for config
    // config: 111, 1SHOT = 0, Resolution [r2,r1,r0] = 001, SD = 0 for continuous temp conversion
    spiSendReceive(0b11100010); // see DS1722 datasheet p6
    digitalWrite(CS_PIN, PIO_LOW);
}

void writeDS1722(char data){
    digitalWrite(CS_PIN, PIO_HIGH);
    spiSendReceive(0x80); // write address for config
    // config: 111, 1SHOT = 0, Resolution [r2,r1,r0], SD = 0 for continuous temp conversion
    spiSendReceive(data);
    digitalWrite(CS_PIN, PIO_LOW);
}

float readDS1722(){
    // read decimals @address 01h [2^-1, 2^-2, 2^-3, 2^-4, 0,0,0,0] (DS p5)
    digitalWrite(CS_PIN, PIO_HIGH); 
    spiSendReceive(0x01); 
    int t_lsb = spiSendReceive(0x00); // send doesn't matter for reads, just need the recieved 8 bits
    digitalWrite(CS_PIN, PIO_LOW); 

    // read integers @address 02h, [2^6 to 2^0]
    digitalWrite(CS_PIN, PIO_HIGH);
    spiSendReceive(0x02);
    int t_msb = spiSendReceive(0x00);
    digitalWrite(CS_PIN, PIO_LOW);

    if (t_msb >= 128) {
        t_msb -= 256;
    }

    float temperature = t_msb + (t_lsb / 256.0f); // gotta make sure t_lsb is /2^8 to get the decimal value it represents

    return temperature;
}