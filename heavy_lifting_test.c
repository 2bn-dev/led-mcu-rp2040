#include <stdio.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "2bn-mcumz16163-a-pinout.pio.h"


bool STATUS_LED_STATE = false;
int dim = 0;
bool dim_polarity = true;
int color_red = 0;
int color_green = 0;
int color_blue = 0;

int main() {
	stdio_init_all();

	//TODO: not super random...
	srand(7);
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

	gpio_put(PIN_OE, 1);
	printf("start\n");
	while (true) {
		for(int pwm = 0; pwm < 256; pwm++){
			for(uint y = 0; y < 16; y++){
				for(uint x = 0; x < 16; x++){
					int chance = (x*y);
					if((chance/8)+color_red > pwm+dim){
						//red
						gpio_put(PIN_SDI_RED, 1);
					}else{
						gpio_put(PIN_SDI_RED, 0);
					}

					if((chance/8)+color_blue > pwm+dim){
						gpio_put(PIN_SDI_BLUE, 1);
					}else{
						gpio_put(PIN_SDI_BLUE, 0);
					}

					if((chance/8)+color_green > pwm+dim){
						gpio_put(PIN_SDI_GREEN, 1);
					}else{
						gpio_put(PIN_SDI_GREEN, 0);
					}
				
					if(x == y){ // confusing, but works.
						gpio_put(PIN_SDI_ROW, 0);
					}else{
						gpio_put(PIN_SDI_ROW, 1);
					}
	
					gpio_put(PIN_CLK, 1);
					gpio_put(PIN_CLK, 0);
				}

				gpio_put(PIN_OE, 1);

				gpio_put(PIN_LE, 1);
				gpio_put(PIN_LE_ROW, 1);
				gpio_put(PIN_LE, 0);
				gpio_put(PIN_LE_ROW, 0);
	
	
				gpio_put(PIN_OE, 0);
				if(STATUS_LED_STATE){
					gpio_put(PIN_STATUS_LED, 0);
					STATUS_LED_STATE = false;
				}else{
					gpio_put(PIN_STATUS_LED, 1);
					STATUS_LED_STATE = true;
				}

			}


		}
		if(dim_polarity){
			dim++;
		}else{
			dim--;
		}
		if(dim > 256 || dim < 0){
			dim_polarity = !dim_polarity;
			color_red += rand()/(RAND_MAX/32);
			color_green += rand()/(RAND_MAX/32);
			color_blue += rand()/(RAND_MAX/32);
			printf("%d %d %d\n", color_red, color_green, color_blue);
			if(color_red > 256){
				color_red = 0;
				printf("red reset\n");
			}
			if(color_green > 256){
				color_green = 0;
			}
			if(color_blue > 256){
				color_blue = 0;
			}
		}
		
	}
	return 0;
}
