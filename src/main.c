/*
File: main.c
Author: Jessica Li
Email: jesli@hmc.edu
Date: 10/18/2026
*/


#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "main.h"
#include "DS1722.h"

/////////////////////////////////////////////////////////////////
// Provided Constants and Functions
/////////////////////////////////////////////////////////////////

//Defining the web page in two chunks: everything before the current time, and everything after the current time
char* webpageStart = "<!DOCTYPE html><html><head><title>E155 Web Server Demo Webpage</title>\
	<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\
	</head>\
	<body><h1>E155 Web Server Demo Webpage</h1>";
char* ledStr = "<p>LED Control:</p> <form action=\"ledon\"><input type=\"submit\" value=\"Turn the LED on!\"></form>\
	<form action=\"ledoff\"><input type=\"submit\" value=\"Turn the LED off!\"></form>";
char* resStr = "<p>Resolution Control:</p> <form action=\"8\"><input type=\"submit\" value=\"8 bits\"></form>\
  <form action=\"9\"><input type=\"submit\" value=\"9 bits\"></form>\
  <form action=\"10\"><input type=\"submit\" value=\"10 bits\"></form>\
  <form action=\"11\"><input type=\"submit\" value=\"11 bits\"></form>\
  <form action=\"12\"><input type=\"submit\" value=\"12 bits\"></form>";
char* webpageEnd   = "</body></html>";

//determines whether a given character sequence is in a char array request, returning 1 if present, -1 if not present
int inString(char request[], char des[]) {
	if (strstr(request, des) != NULL) {return 1;}
	return -1;
}

int updateLEDStatus(char request[])
{
	int led_status = 0;
	// The request has been received. now process to determine whether to turn the LED on or off
	if (inString(request, "ledoff")==1) {
		digitalWrite(LED_PIN, PIO_LOW);
		led_status = 0;
	}
	else if (inString(request, "ledon")==1) {
		digitalWrite(LED_PIN, PIO_HIGH);
		led_status = 1;
	}

	return led_status;
}

char getResConfig(char request[], char currentConfig)
{
	char config = currentConfig;

  if (inString(request, "/REQ:8")==1) {
		config = 0b11100000; // [rs2, rs1, rs0] = 000
	}
	else if (inString(request, "/REQ:9")==1) {
		config = 0b11100010;
	}
  else if (inString(request, "/REQ:10")==1) {
		config = 0b11100100;
	}
  else if (inString(request, "/REQ:11")==1) {
		config = 0b11100110;
	}
  else if (inString(request, "/REQ:12")==1) {
		config = 0b11101000;
	}

	return config;
}

/////////////////////////////////////////////////////////////////
// Solution Functions
/////////////////////////////////////////////////////////////////

int main(void) {
  configureFlash();
  configureClock();

  gpioEnable(GPIO_PORT_A);
  gpioEnable(GPIO_PORT_B);
  gpioEnable(GPIO_PORT_C);

  pinMode(LED_PIN, GPIO_OUTPUT);
  
  RCC->APB2ENR |= (RCC_APB2ENR_TIM15EN);
  RCC->APB2ENR |= (RCC_APB2ENR_TIM16EN);
  initTIM(TIM15);
  initTIM(TIM16);
  
  USART_TypeDef * USART = initUSART(USART1_ID, 125000);

  //SPI & sensor initialization 
  initSPI(0b101, 0, 1); // CLK Phase must be 1 (DS DS1722 p7), baud rate: fPLCK/64 = 1.25 Mhz
  initDS1722();

  char resConfig = 0xE2;
  char tempStr[20];
  char resConfigStr[20];


  while(1) {
    /* Wait for ESP8266 to send a request.
    Requests take the form of '/REQ:<tag>\n', with TAG begin <= 10 characters.
    Therefore the request[] array must be able to contain 18 characters.
    */

    // Receive web request from the ESP
    char request[BUFF_LEN] = "                  "; // initialize to known value
    int charIndex = 0;
  
    // Keep going until you get end of line character
    while(inString(request, "\n") == -1) {
      // Wait for a complete request to be transmitted before processing
      while(!(USART->ISR & USART_ISR_RXNE));
      request[charIndex++] = readChar(USART);
    }

    // set temp resolution over SPI
    char newConfig = getResConfig(request, resConfig);

    if (newConfig != resConfig){
      resConfig = newConfig;
      writeDS1722(resConfig);
      //delay_millis(TIM16, 750);
    }
    
    // Read temp (SPI)
    float temp = readDS1722();
  
    // Update string with current LED state
    int led_status = updateLEDStatus(request);

    char ledStatusStr[20];
    if (led_status == 1)
      sprintf(ledStatusStr,"LED is on!");
    else if (led_status == 0)
      sprintf(ledStatusStr,"LED is off!");

    // print temperature
    sprintf(tempStr, "%f C", temp);
    // sprintf(resConfigStr, "Request: %s", request);
    sprintf(resConfigStr, "Res Config: 0x%02X", (unsigned char)resConfig);

    // finally, transmit the webpage over UART
    sendString(USART, webpageStart); // webpage header code
    sendString(USART, ledStr); // button for controlling LED
    sendString(USART, resStr); // buttons for controlling temp resolution

    sendString(USART, "<h2>LED Status</h2>");
    sendString(USART, "<p>");
    sendString(USART, ledStatusStr);
    sendString(USART, "</p>");

    sendString(USART, "<h2>Temperature</h2>");
    sendString(USART, "<p>");
    sendString(USART, tempStr);
    sendString(USART, "</p>");

    sendString(USART, "<h2>Resolution</h2>");
    sendString(USART, "<p>");
    sendString(USART, resConfigStr);
    sendString(USART, "</p>");

  
    sendString(USART, webpageEnd);
  }
}
