/**
 * Pi Pico GPS Clock using NEO 6M Module with MAX78219 Chip Driving 8 7-Segment Units via SPI
 * By Suvir Kumar <suvir@talkkonnect.com> 
 * Released 04/02/2022
 * 
 * BSD License 3 Clause Below SPI Code Copied from max7219_8x7seg_spi.c pi pico examples
 *
 *   
 * Copyright (c) 2022 Raspberry Pi (Trading) Ltd.
 * SPDX-License-Identifier: BSD-3-Clause
 * NOTE: The device is driven at 5v, but SPI communications are at 3v3
 * Using SPI Board On Port SPI-1 of PI PICO
 * GPIO 13 (pin ) Chip select -> CS on Max7219 board
 * GPIO 10 (pin ) SCK/spi0_sclk -> CLK on Max7219 board
 * GPIO 11 (pin ) MOSI/spi0_tx -> DIN on Max7219 board
 * 5v (pin 40) -> VCC on Max7219 board
 * GND (pin 38)  -> GND on Max7219 board
 */

#include "hardware/spi.h"
#include "hardware/uart.h"
#include "pico/binary_info.h"
#include "pico/stdlib.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>

#define NUM_MODULES 1

#define UART0_ID uart0
#define BAUD0_RATE 9600
#define DATA0_BITS 8
#define STOP0_BITS 1
#define PARITY0 UART_PARITY_NONE
#define UART0_TX_PIN 0
#define UART0_RX_PIN 1

#define UART1_ID uart1
#define BAUD1_RATE 9600
#define DATA1_BITS 8
#define STOP1_BITS 1
#define PARITY1 UART_PARITY_NONE
#define UART1_TX_PIN 8
#define UART1_RX_PIN 9

#define BUFFSIZE 100

const uint8_t CMD_NOOP = 0;
const uint8_t CMD_DIGIT0 = 1; // Goes up to 8, for each line
const uint8_t CMD_DECODEMODE = 9;
const uint8_t CMD_BRIGHTNESS = 10;
const uint8_t CMD_SCANLIMIT = 11;
const uint8_t CMD_SHUTDOWN = 12;
const uint8_t CMD_DISPLAYTEST = 15;
const int LOCALTIMEZONE = 7; //Define your Hour OFFSet From UTC Here

#ifdef PICO_DEFAULT_SPI_CSN_PIN
static inline void cs_select() {
  asm volatile("nop \n nop \n nop");
  gpio_put(PICO_DEFAULT_SPI_CSN_PIN, 0); // Active low
  asm volatile("nop \n nop \n nop");
}

static inline void cs_deselect() {
  asm volatile("nop \n nop \n nop");
  gpio_put(PICO_DEFAULT_SPI_CSN_PIN, 1);
  asm volatile("nop \n nop \n nop");
}
#endif

#if defined(spi_default) && defined(PICO_DEFAULT_SPI_CSN_PIN)
static void write_register(uint8_t reg, uint8_t data) {
  uint8_t buf[2];
  buf[0] = reg;
  buf[1] = data;
  cs_select();
  spi_write_blocking(spi_default, buf, 2);
  cs_deselect();
  sleep_ms(1);
}

static void write_register_all(uint8_t reg, uint8_t data) {
  uint8_t buf[2];
  buf[0] = reg;
  buf[1] = data;
  cs_select();
  for (int i = 0; i < NUM_MODULES; i++) {
    spi_write_blocking(spi_default, buf, 2);
  }
  cs_deselect();
}
#endif

void display_num(int32_t num) {
  int digit = 0;
  while (num && digit < 8) {
    write_register_all(CMD_DIGIT0 + digit, num % 10);
    num /= 10;
    digit++;
  }
}

void clear() {
  for (int i = 0; i < 8; i++) {
    write_register_all(CMD_DIGIT0 + i, 0);
  }
}

int main() {

  char buffer[BUFFSIZE] = "";
  char timed1[BUFFSIZE] = "";
  char timed2[BUFFSIZE] = "";
  char timed3[BUFFSIZE] = "";
  char timed4[BUFFSIZE] = "";
  char timed5[BUFFSIZE] = "";
  char timed6[BUFFSIZE] = "";
  char utchours[BUFFSIZE] = "";
  char localhourss[BUFFSIZE] = "";
  int i = 0;
  int j = 0;
  bool endchar1 = false;
  bool endchar2 = false;
  int localhours = 0;
  char c;
  char *ptr;

  uart_init(UART0_ID, BAUD0_RATE);
  gpio_set_function(UART0_TX_PIN, UART0_TX_PIN);
  gpio_set_function(UART0_RX_PIN, UART0_RX_PIN);
  uart_set_hw_flow(UART0_ID, false, false);
  uart_set_format(UART0_ID, DATA0_BITS, STOP0_BITS, PARITY0);

  uart_init(UART1_ID, BAUD1_RATE);
  gpio_set_function(UART1_TX_PIN, UART1_TX_PIN);
  gpio_set_function(UART1_RX_PIN, UART1_RX_PIN);
  uart_set_hw_flow(UART1_ID, false, false);
  uart_set_format(UART1_ID, DATA1_BITS, STOP1_BITS, PARITY1);

  gpio_set_function(0, GPIO_FUNC_UART);
  gpio_set_function(1, GPIO_FUNC_UART);
  gpio_set_function(8, GPIO_FUNC_UART);
  gpio_set_function(9, GPIO_FUNC_UART);

  // This example will use SPI0 at 10MHz.
  spi_init(spi_default, 10 * 1000 * 1000);
  gpio_set_function(PICO_DEFAULT_SPI_SCK_PIN, GPIO_FUNC_SPI);
  gpio_set_function(PICO_DEFAULT_SPI_TX_PIN, GPIO_FUNC_SPI);

  // Make the SPI pins available to picotool
  bi_decl(bi_2pins_with_func(PICO_DEFAULT_SPI_TX_PIN, PICO_DEFAULT_SPI_SCK_PIN, GPIO_FUNC_SPI));

  // Chip select is active-low, so we'll initialise it to a driven-high state
  gpio_init(PICO_DEFAULT_SPI_CSN_PIN);
  gpio_set_dir(PICO_DEFAULT_SPI_CSN_PIN, GPIO_OUT);
  gpio_put(PICO_DEFAULT_SPI_CSN_PIN, 1);

  // Make the CS pin available to picotool
  bi_decl(bi_1pin_with_name(PICO_DEFAULT_SPI_CSN_PIN, "SPI CS"));

  // Send init sequence to device
  write_register_all(CMD_SHUTDOWN, 0);
  write_register_all(CMD_DISPLAYTEST, 0);
  write_register_all(CMD_SCANLIMIT, 7);
  write_register_all(CMD_DECODEMODE, 255);
  write_register_all(CMD_SHUTDOWN, 1);
  write_register_all(CMD_BRIGHTNESS, 8);

  clear();

  while (true) {
    while (uart_is_readable(uart0)) {
      c = uart_getc(uart0);
      i++;
      if (c == 10) {
        endchar1 = true;
        endchar2 = false;
      }
      if ((c == 13) && (endchar1 == true)) {
        endchar2 = true;
      }
      // Find End of Sentence
      if (endchar1 == true && endchar2 == true) {
        buffer[i] = '\0';
        // strip out the UTC time from the GGA Sentence
        if (strstr(buffer, "GGA")) {
          //uart_puts(UART1_ID, buffer);
          strncpy(utchours, buffer + 9, 2);
 
          localhours = strtol(utchours, &ptr, 10) + LOCALTIMEZONE;
          if (localhours > 23) {
            localhours = localhours - 24;
          }
 
          sprintf(localhourss, "%d", localhours);
          //uart_puts(UART1_ID, localhourss);

          strncpy(timed6, localhourss, 1);
          strncpy(timed5, localhourss + 1, 1);
          strncpy(timed4, buffer + 11, 1);
          strncpy(timed3, buffer + 12, 1);
          strncpy(timed2, buffer + 13, 1);
          strncpy(timed1, buffer + 14, 1);

          write_register(1, strtol(timed1, &ptr, 10));
          write_register(2, strtol(timed2, &ptr, 10));
          write_register(3, 15); // show blank display
          write_register(4, strtol(timed3, &ptr, 10));
          write_register(5, strtol(timed4, &ptr, 10));
          write_register(6, 15); //show blank display 
          write_register(7, strtol(timed5, &ptr, 10));
          write_register(8, strtol(timed6, &ptr, 10));
        }
        endchar1 == false;
        endchar2 == false;
        i=0;
      }

      if (i < BUFFSIZE - 1) {
        buffer[i] = c;
      }
    }
    sleep_ms(1);
  }
}
