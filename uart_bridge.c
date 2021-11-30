#include <stdlib.h>
#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/stdio.h"
#include "hardware/uart.h"
#include "tusb.h"
#include "device/usbd_pvt.h"

#define PIN_TXD0 0
#define PIN_RXD0 1
#define PIN_ESP32_EN 13
#define PIN_ESP32_FLASH 14

uint32_t current_baud_rate;
static void uart0_init(uint32_t baud_rate){
	current_baud_rate = uart_init(uart0, baud_rate);
	printf("init: %d %d\n" , (uint32_t *)baud_rate, current_baud_rate);
	//uart_set_format(uart0, 8, 0, 1)
	uart_set_translate_crlf(uart0, false);
}
static void uart0_deinit(){
	uart_deinit(uart0);
	current_baud_rate = 0;
}
static void pin_init(void){
	gpio_set_function(PIN_TXD0, GPIO_FUNC_UART);
        gpio_set_function(PIN_RXD0, GPIO_FUNC_UART);
        gpio_init(PIN_ESP32_FLASH);
        gpio_init(PIN_ESP32_EN);
        gpio_pull_up(PIN_ESP32_FLASH);
	sleep_ms(1);
	gpio_pull_up(PIN_ESP32_EN);
	sleep_ms(1);
	gpio_pull_down(PIN_ESP32_EN);
}



/* From ESPTOOL:
 * # issue reset-to-bootloader:
 * # RTS = either CH_PD/EN or nRESET (both active low = chip in reset
 * # DTR = GPIO0 (active low = boot to flasher)
 * #
 * # DTR & RTS are active low signals,
 * # ie True = pin @ 0V, False = pin @ VCC.
 *
 * (Note: 2bn LED MCU uses inverter for EN and FLASH pins between RP2040 and ESP32).
 *
 * gpio_pull_up(PIN_ESP32_FLASH); == bootloader mode
 * gpio_pull_up(PIN_ESP32_EN);    == chip disable
 * gpio_pull_down(PIN_ESP32_EN);  == chip enable
 */

uint8_t last_rts = 1;
uint8_t last_dtr = 1;

int main(){
	uart0_init(115200);
	pin_init();
        stdio_usb_init();
	setvbuf(stdout, NULL, _IONBF, 0);
	char *outbuf = (char *) malloc(sizeof(char)*1024);
	//setbuf(stdout, NULL);
        while(true){
		//cdc_line_coding_t* line_coding;
		//tud_cdc_n_get_line_coding(0, line_coding);
		//printf("%d %d %d %d\n", line_coding->bit_rate, line_coding->stop_bits, line_coding->parity, line_coding->data_bits);
		//if(*ptr != current_baud_rate){
			//uart0_deinit();
			//uart0_init( *ptr);
		//}
		uint8_t line_state = tud_cdc_n_get_line_state(0); //#define USBD_ITF_CDC       (0)
                uint8_t dtr = line_state & 0b1;
                uint8_t rts = line_state & 0b10>>1;
		/*
                if(dtr == 1 && last_dtr != 1){
                        gpio_pull_down(PIN_ESP32_FLASH);
                }else if(dtr == 0 && last_dtr != 0){
                        gpio_pull_up(PIN_ESP32_FLASH);
                }
		last_dtr = dtr;
		*/
                if(rts == 1 && last_rts != 1){
                        gpio_pull_down(PIN_ESP32_EN);
                }else if(rts == 0 && last_rts != 0 && dtr == 1) {
                        gpio_pull_up(PIN_ESP32_EN);
			//uart0_init();
                }
		last_rts = rts;
		stdio_flush();
		while(true){
			int32_t buf;
			buf = getchar_timeout_us(0);
			if(buf == PICO_ERROR_TIMEOUT){
				break;
			}

			uart_putc_raw(uart0, buf);
		}
		uint32_t len = 0;
		while(uart_is_readable(uart0)){
                        outbuf[len] = uart_getc(uart0);
			len++;
			if(len > 1023){
				break;
			}
                }
		if( len > 0){
			outbuf[len] = '\0';
			fputs(outbuf, stdout);
		}
		uart_tx_wait_blocking(uart0);
		

        }

}
