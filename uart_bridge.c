
#include <stdlib.h>
#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/stdio.h"
#include "hardware/uart.h"
#include "device/usbd_pvt.h"

#define PIN_TXD0 0
#define PIN_RXD0 1
#define PIN_ESP32_EN 13
#define PIN_ESP32_FLASH 14

const char ESP_SYNC[] = { 
	0xc0, 0x00, 0x08, 0x24, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x07, 0x07, 0x12, 0x20, 0x55, 0x55, 0x55,
       	0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 
	0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 
	0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 
	0x55, 0x55, 0x55, 0x55, 0x55, 0xC0
};

uint32_t current_baud_rate;
static void uart0_init(uint32_t baud_rate){
	current_baud_rate = uart_init(uart0, baud_rate);
	uart_set_format(uart0, 8, 1, UART_PARITY_NONE);
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
        gpio_pull_down(PIN_ESP32_FLASH);
	sleep_ms(1);
	gpio_pull_up(PIN_ESP32_EN);
	sleep_ms(1);
	gpio_pull_down(PIN_ESP32_EN);
	//This resets ESP32
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


/* Sequencing from ESPTOOL (seconds):
 * DTR: 0.0000000 False == TINYUSB 0 ?
 * RTS: 0.0007728 True == TINYUSB 1
 * DTR: 0.1010019 True 
 * RTS: 0.1040874 False
 * DTR: 0.1542244 False
 */


int main(){
	stdio_usb_init();
	uart0_init(115200);
	pin_init();

	char read_buf[128];
	int32_t read_buf_write = 0;
	bool match_found = false;
        while(true){
		while(uart_is_readable(uart0)){
		        int32_t buf = uart_getc(uart0);
		        if(buf >= 0){
				putchar_raw(buf);
			}
		}
		stdio_flush();
		uart_tx_wait_blocking(uart0);
		while(uart_is_writable(uart0)){
			int32_t buf;
			buf = getchar_timeout_us(0);
			if(buf < 0){
				break;
			}
			uart_putc_raw(uart0, buf);


			read_buf[read_buf_write] = buf;
			read_buf_write++;
			if(read_buf_write >= sizeof(read_buf)){
				read_buf_write = 0;
				
			}

			if(!match_found){
				for(int32_t i = 0; i < sizeof(read_buf); i++){
					if(read_buf[i] == 0xC0){
						int32_t i_orig = i;
						int32_t ii = 0;
						for(; ii < sizeof(ESP_SYNC); ii++){
							if(read_buf[i] != ESP_SYNC[ii]){
								break;
							}else{
								i++;
								if(i >= sizeof(read_buf)){
									i = 0;
								}
							}
						}
						if(ii == sizeof(ESP_SYNC)-1){
							//ESP Sync magic found, enter flash mode
							gpio_pull_up(PIN_ESP32_FLASH);
							gpio_pull_up(PIN_ESP32_EN);
							sleep_ms(1);
							gpio_pull_down(PIN_ESP32_EN);
							match_found = true;
							break;
						}else{
							i = i_orig;
						}
					}
				}
			}
		}
		stdio_flush();
		

        }

}
