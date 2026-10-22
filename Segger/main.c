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
// Compact, modern inline CSS embedded directly in C string literals
char* webpageStart = 
  "<!DOCTYPE html><html><head>"
  "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
  "<style>"
  "body{font-family:system-ui,sans-serif;background:#f0f2f5;color:#1c1e21;margin:0;padding:16px;display:flex;justify-content:center}"
  ".card{background:#fff;border-radius:12px;padding:20px;max-width:400px;width:100%;box-shadow:0 4px 12px rgba(0,0,0,0.08)}"
  "h1{font-size:20px;margin:0 0 16px;color:#007aff;text-align:center}"
  "h2{font-size:14px;text-transform:uppercase;letter-spacing:0.5px;color:#65676b;margin:16px 0 6px}"
  ".val{font-size:22px;font-weight:600;margin:0;padding:10px;background:#f7f8fa;border-radius:8px}"
  ".btn-group{display:flex;gap:8px;flex-wrap:wrap;margin-top:6px}"
  "form{flex:1;min-width:70px;margin:0}"
  "input[type=submit]{width:100%;padding:10px;border:none;border-radius:6px;background:#007aff;color:#fff;font-weight:600;cursor:pointer}"
  "input[type=submit]:hover{opacity:0.9}"
  ".off{background:#8e8e93}"
  "</style></head><body><div class=\"card\">"
  "<h1>E155 Temp Monitor</h1>";

char* ledStr = 
  "<h2>LED Control</h2>"
  "<div class=\"btn-group\">"
  "<form action=\"ledon\"><input type=\"submit\" value=\"ON\"></form>"
  "<form action=\"ledoff\"><input type=\"submit\" class=\"off\" value=\"OFF\"></form>"
  "</div>";

char* resStr = 
  "<h2>Resolution Control</h2>"
  "<div class=\"btn-group\">"
  "<form action=\"REQ:8\"><input type=\"submit\" value=\"8-bit\"></form>"
  "<form action=\"REQ:9\"><input type=\"submit\" value=\"9-bit\"></form>"
  "<form action=\"REQ:10\"><input type=\"submit\" value=\"10-bit\"></form>"
  "<form action=\"REQ:11\"><input type=\"submit\" value=\"11-bit\"></form>"
  "<form action=\"REQ:12\"><input type=\"submit\" value=\"12-bit\"></form>"
  "</div>";

char* webpageEnd = "</div></body></html>";

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
  initTIM(TIM15);
  
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

    // transmit the webpage over UART
    sendString(USART, webpageStart);
    sendString(USART, ledStr);
    sendString(USART, resStr);

    sendString(USART, "<h2>LED Status</h2>");
    sendString(USART, "<p class=\"val\">");
    sendString(USART, ledStatusStr);
    sendString(USART, "</p>");

    sendString(USART, "<h2>Temperature</h2>");
    sendString(USART, "<p class=\"val\">");
    sendString(USART, tempStr);
    sendString(USART, "</p>");

    sendString(USART, "<h2>Resolution</h2>");
    sendString(USART, "<p class=\"val\">");
    sendString(USART, resConfigStr);
    sendString(USART, "</p>");

    sendString(USART, webpageEnd);
  }
}
