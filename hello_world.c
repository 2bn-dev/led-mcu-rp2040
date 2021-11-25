#include <stdio.h>
#include "pico/stdlib.h"
#include "2bn-mcumz16163-a.pio.h"


bool STATUS_LED_STATE = false;

uint color;
uint pos_x;
uint pos_y;

int main() {
	pos_x = 0;
	pos_y = 0;
	color = 0;
	stdio_init_all();

	gpio_init(PIN_STATUS_LED);
	gpio_init(PIN_SDI_RED);
	gpio_init(PIN_SDI_BLUE);
	gpio_init(PIN_SDI_GREEN);
	gpio_init(PIN_SDI_ROW);
	gpio_init(PIN_LE);
	gpio_init(PIN_CLK);
	gpio_init(PIN_OE);
	gpio_init(PIN_LE_ROW);

	gpio_set_dir(PIN_STATUS_LED, GPIO_OUT);
	gpio_set_dir(PIN_SDI_RED, GPIO_OUT);
	gpio_set_dir(PIN_SDI_BLUE, GPIO_OUT);
	gpio_set_dir(PIN_SDI_GREEN, GPIO_OUT);
	gpio_set_dir(PIN_SDI_ROW, GPIO_OUT);
	gpio_set_dir(PIN_LE, GPIO_OUT);
	gpio_set_dir(PIN_CLK, GPIO_OUT);
	gpio_set_dir(PIN_OE, GPIO_OUT);
	gpio_set_dir(PIN_LE_ROW, GPIO_OUT);



	while (true) {
		for(uint y = 0; y < 16; y++){
			for(uint x = 0; x < 16; x++){
				if(x == pos_x && y == pos_y){
					if(color < 100){
						//red
						gpio_put(PIN_SDI_RED, 1);
						gpio_put(PIN_SDI_BLUE, 0);
						gpio_put(PIN_SDI_GREEN, 0);
					}else if(color < 200){
						//green
						gpio_put(PIN_SDI_RED, 0);
						gpio_put(PIN_SDI_BLUE, 0);
						gpio_put(PIN_SDI_GREEN, 1);
					}else if(color < 300){
						//blue
						gpio_put(PIN_SDI_RED, 0);
						gpio_put(PIN_SDI_BLUE, 1);
						gpio_put(PIN_SDI_GREEN, 0);
					}else if(color < 400){
						//white
						gpio_put(PIN_SDI_RED, 1);
						gpio_put(PIN_SDI_BLUE, 1);
						gpio_put(PIN_SDI_GREEN, 1);
					}else{
						//off
						gpio_put(PIN_SDI_RED, 0);
						gpio_put(PIN_SDI_BLUE, 0);
						gpio_put(PIN_SDI_GREEN, 0);
					}
				}else{
					gpio_put(PIN_SDI_RED, 0);
					gpio_put(PIN_SDI_BLUE, 0);
					gpio_put(PIN_SDI_GREEN, 0);
				}

				
				if(x == pos_y){ // confusing, but works.
					gpio_put(PIN_SDI_ROW, 0);
				}else{
					gpio_put(PIN_SDI_ROW, 1);
				}

				sleep_us(1);
				gpio_put(PIN_CLK, 1);
				sleep_us(1);
				gpio_put(PIN_CLK, 0);
				sleep_us(1);
			}

			gpio_put(PIN_LE, 1);
			sleep_us(1);
			gpio_put(PIN_LE, 0);
			sleep_us(1);
			gpio_put(PIN_LE_ROW, 1);
			sleep_us(1);
			gpio_put(PIN_LE_ROW, 0);


			gpio_put(PIN_OE, 0);
			sleep_us(1);
			gpio_put(PIN_OE, 1);
			sleep_us(9);

			if(STATUS_LED_STATE){
				gpio_put(PIN_STATUS_LED, 0);
				STATUS_LED_STATE = false;
			}else{
				gpio_put(PIN_STATUS_LED, 1);
				STATUS_LED_STATE = true;
			}

		}


		color += 1;
		if(color > 500){
			color = 0;
			pos_x += 1;
			if(pos_x > 15){
				pos_x = 0;
				pos_y += 1;
				if(pos_y > 15){
					pos_y = 0;
				}
			}
			printf("%d %d\n", pos_x, pos_y);
		}
	}

	return 0;
}
