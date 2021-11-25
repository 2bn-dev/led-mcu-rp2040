#include <stdio.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "2bn-mcumz16163-a-pinout.pio.h"
#include "led_dma.pio.h"
#include "pico/time.h"
#include <inttypes.h>



bool STATUS_LED_STATE = false;
int dim = 0;
bool dim_polarity = true;
int color_red = 0;
int color_green = 0;
int color_blue = 0;

absolute_time_t start_time = 0;
absolute_time_t end_time = 0;


int main() {
	stdio_init_all();

	//TODO: not super random...
	srand(7);

	PIO pio = pio0;
	uint offset = pio_add_program(pio, &led_pio_program);
	uint sm = pio_claim_unused_sm(pio, true);
	led_pio_program_init(pio, sm, offset);


	
	gpio_init(PIN_STATUS_LED);
	/*
	gpio_init(PIN_SDI_RED);
	gpio_init(PIN_SDI_BLUE);
	gpio_init(PIN_SDI_GREEN);
	gpio_init(PIN_SDI_ROW);
	gpio_init(PIN_LE);
	gpio_init(PIN_CLK);
	gpio_init(PIN_OE);
	gpio_init(PIN_LE_ROW);
	*/

	gpio_set_dir(PIN_STATUS_LED, GPIO_OUT);

	/*
	gpio_set_dir(PIN_SDI_RED, GPIO_OUT);
	gpio_set_dir(PIN_SDI_BLUE, GPIO_OUT);
	gpio_set_dir(PIN_SDI_GREEN, GPIO_OUT);
	gpio_set_dir(PIN_SDI_ROW, GPIO_OUT);
	gpio_set_dir(PIN_LE, GPIO_OUT);
	gpio_set_dir(PIN_CLK, GPIO_OUT);
	gpio_set_dir(PIN_OE, GPIO_OUT);
	gpio_set_dir(PIN_LE_ROW, GPIO_OUT);

	gpio_put(PIN_OE, 1);
	*/

	uint32_t buf = 0b0;
	printf("start\n");
	while (true) {
		start_time = get_absolute_time();
		for(int pwm = 0; pwm < 64; pwm++){
			for(uint y = 0; y < 16; y++){
				uint8_t local_buf = 0b0;
				for(uint x = 0; x < 16; x++){
					if(buf != 0b0 && x%2 == 0){
						pio_sm_put(pio, sm, buf);
						buf = 0b0;
					}

					int chance = (x*y);


					int brightness_limit = pwm+dim;
					//if(brightness_limit%(color_red+(chance/4)) <1){
					if(0){
						//red
						//gpio_put(PIN_SDI_RED, 1);
						local_buf = 0b1;
					}else{
						//gpio_put(PIN_SDI_RED, 0);
						local_buf = 0b0;
					}

					/*if(brightness_limit%(brightness_limit/(chance+color_blue)/8) == 0){
						//gpio_put(PIN_SDI_BLUE, 1);
						local_buf |= (0b1 << 1);
					}else{
						//gpio_put(PIN_SDI_BLUE, 0);
						local_buf &= ~(0b1 << 1);
					}

					if(brightness_limit%(brightness_limit/(chance+color_green)/8) == 0){
						//gpio_put(PIN_SDI_GREEN, 1);
						local_buf |= (0b1 << 2);
					}else{
						//gpio_put(PIN_SDI_GREEN, 0);
						local_buf &= ~(0b1 << 2);
					}
					*/
				
					if(x == y){ // confusing, but works.
						//gpio_put(PIN_SDI_ROW, 0);
						local_buf &= ~(0b1 << 3);
					}else{
						//gpio_put(PIN_SDI_ROW, 1);
						local_buf |= (0b1 << 3);
					}
					local_buf &= ~(0b1 << 4); // LE
					local_buf |=  (0b1 << 5); // CLK
					local_buf &= ~(0b1 << 6); // OE
					local_buf &= ~(0b1 << 7); // LE_ROW
					if(x%2 == 0){
						buf |= (local_buf << 24); // CLK HIGH
						local_buf &= ~(0b1 << 5); // CLK
						buf |= (local_buf << 16); //CLK LOW
					}else{
						buf |= (local_buf << 8); // CLK HIGH
						local_buf &= ~(0b1 << 5); // CLK
						buf |= (local_buf << 0); //CLK LOW
					}
					//gpio_put(PIN_CLK, 1);
					//gpio_put(PIN_CLK, 0);
				}

				if(buf != 0b0){
	                                pio_sm_put(pio, sm, buf);
					buf = 0b0;
				}


				local_buf |= (0b1 << 6); // OE
				//gpio_put(PIN_OE, 1);

				//gpio_put(PIN_LE, 1);
				//gpio_put(PIN_LE_ROW, 1);
				local_buf |= (0b1 << 4); // LE
				local_buf |= (0b1 << 7); // LE_ROW

				buf |= (local_buf << 24);
				buf |= (local_buf << 16);
				


				//gpio_put(PIN_LE, 0);
				//gpio_put(PIN_LE_ROW, 0);
				local_buf &= ~(0b1 << 4); // LE
                                local_buf &= ~(0b1 << 7); // LE_ROW
				
	
				//gpio_put(PIN_OE, 0);
				local_buf &= ~(0b1 << 6); // OE

				buf |= (local_buf << 8);
				buf |= (local_buf << 0);

				pio_sm_put(pio, sm, buf);
				buf = 0b0;  


			}

		}
		end_time = get_absolute_time();
		if(STATUS_LED_STATE){
	                gpio_put(PIN_STATUS_LED, 0);
	                STATUS_LED_STATE = false;
	        }else{
	                gpio_put(PIN_STATUS_LED, 1);
	                STATUS_LED_STATE = true;
	        }

		if(dim_polarity){
			dim++;
		}else{
			dim--;
		}
		if(dim > 128 || dim < -64){
			dim_polarity = !dim_polarity;
			color_red += rand()/(RAND_MAX/32);
			color_green += rand()/(RAND_MAX/32);
			color_blue += rand()/(RAND_MAX/32);
			printf("%" PRId64 " ", absolute_time_diff_us(start_time, end_time));
			printf(" %d %d %d\n", color_red, color_green, color_blue);
			if(color_red > 256){
				color_red = 0;
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
