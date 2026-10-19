// DS1722.h
/*
Author: Jessica Li
Email: jesli@hmc.edu
Date: 10/17/2026
Header file for functions for temp sensor DS1722
*/

#ifndef DS1722_H
#define DS1722_H

#include <stdint.h>
#include <stm32l432xx.h>

///////////////////////////////////////////////////////////////////////////////
// Function prototypes
///////////////////////////////////////////////////////////////////////////////

void initDS1722(void);

void writeDS1722(char data);

float readDS1722(void);

#endif