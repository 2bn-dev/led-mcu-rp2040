#include <stdio.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "2bn-mcumz16163-a-pinout.pio.h"
#include "led_dma.pio.h"
#include "pico/time.h"
#include "pico/platform.h"
#include "hardware/sync.h"
#include "pico/multicore.h"
#include "hardware/dma.h"
#include "hardware/irq.h"
#include <inttypes.h>


#include "hardware/structs/rosc.h"


typedef struct {
	        int32_t pixel[17][17];
} frame_matrix_t;


void __attribute__((noinline, long_call, section(".time_critical"))) render_led_frame(PIO pio, uint sm, frame_matrix_t *frame_matrix);
void __attribute__((noinline, section(".time_critical"))) update_frame_matrix(frame_matrix_t *frame_matrix, uint32_t i);
void __attribute__((noinline, section(".time_critical"))) update_dim();
void __attribute__((noinline, section(".time_critical"))) randomize_color();
void __isr dma_complete_handler();
void dma_init(PIO pio, uint sm);
void core1_entry();


int32_t dim = 0;
int32_t dim_counter = 0;
bool dim_polarity = false;
frame_matrix_t frame_matrix;


/* Bit frame order
 *
 * LE_ROW << 7  15 23 31
 * OE     << 6  14 22 30
 * CLK    << 5  13 21 29
 * LE     << 4  12 20 28
 * ROW    << 3  11 19 27
 * GREEN  << 2  10 18 26
 * BLUE   << 1  9  17 25
 * RED    << 0  8  16 24
 *
 *
 */


// Bit frame positioning in 32 bit packet (4 output bytes in each)
#define BYTE3 0
#define BYTE2 8
#define BYTE1 16
#define BYTE0 24

// Bit frame positioning in 8 bit output data
#define LE_ROW 	7
#define OE 	6
#define CLK 	5
#define LE 	4
#define ROW 	3
#define GREEN 	2
#define BLUE 	1
#define RED 	0

#define DEFAULT_PIXEL_PACKET 0b00000000001000000000000000100000 //CLK on, CLK off, CLK on, CLK off
//#define DEFAULT_LATCH_PACKET 0b01000000010000001101000011010000 //OE ON, LE OE on, LE OE LE_ROW on, OE on
#define DEFAULT_LATCH_PACKET 0b00000000000000001001000010010000
#define DEFAULT_FLUSH_PACKET 0b00000000000000000000000000000000 //OE on 4x

#define MAX_FPS 100
#define MIN_FRAME_TIME 1000000/MAX_FPS //In microseconds

#define DIM_MAX 128
#define DIM_MIN 0
#define DIM_PERIOD 1000000 // in Microseconds
#define DIM_STEPS DIM_MAX-DIM_MIN
#define DIM_COUNTER_DIVISOR 2


uint main() {
	stdio_init_all();


	multicore_launch_core1(core1_entry);

	while(true){
		sleep_ms(100);
	}

	return 0;
}



// Some DMA code copy pasted from https://github.com/raspberrypi/pico-examples/blob/master/pio/ws2812/ws2812_parallel.c
// bit plane content dma channel
#define DMA_CHANNEL 0

// chain channel for configuring main dma channel to output from disjoint 8 word fragments of memory
#define DMA_CB_CHANNEL 1
#define DMA_CHANNEL_MASK (1u << DMA_CHANNEL)
#define DMA_CB_CHANNEL_MASK (1u << DMA_CB_CHANNEL)
#define DMA_CHANNELS_MASK (DMA_CHANNEL_MASK | DMA_CB_CHANNEL_MASK)

void dma_init(PIO pio, uint sm){
	dma_claim_mask(DMA_CHANNELS_MASK);

	dma_channel_config channel_config = dma_channel_get_default_config(DMA_CHANNEL);
	channel_config_set_dreq(&channel_config, pio_get_dreq(pio, sm, true));
	channel_config_set_chain_to(&channel_config, DMA_CB_CHANNEL);
	channel_config_set_irq_quiet(&channel_config, true);
	dma_channel_configure(DMA_CHANNEL,
        		&channel_config,
			&pio->txf[sm],
			NULL, // set by chain
			8, // 8 words for 8 bit planes
			false
			);

	// chain channel sends single word pointer to start of fragment each time
	dma_channel_config chain_config = dma_channel_get_default_config(DMA_CB_CHANNEL);
	dma_channel_configure(DMA_CB_CHANNEL,
                          &chain_config,
                          &dma_channel_hw_addr(
                                  DMA_CHANNEL)->al3_read_addr_trig,  // ch DMA config (target "ring" buffer size 4) - this is (read_addr trigger)
                          NULL, // set later
                          1,
                          false
			);

	irq_set_exclusive_handler(DMA_IRQ_0, dma_complete_handler);
	dma_channel_set_irq0_enabled(DMA_CHANNEL, true);
	irq_set_enabled(DMA_IRQ_0, true);
}

void __isr dma_complete_handler() {
    if (dma_hw->ints0 & DMA_CHANNEL_MASK) {
        // clear IRQ
        dma_hw->ints0 = DMA_CHANNEL_MASK;
        // when the dma is complete we start the reset delay timer
        //if (reset_delay_alarm_id) cancel_alarm(reset_delay_alarm_id);
        //reset_delay_alarm_id = add_alarm_in_us(400, reset_delay_complete, NULL, true);
    }
}

void core1_entry(){
	int32_t seed = 0;
	for( uint8_t i = 0; i <32; i++){
		seed |= (rosc_hw->randombit<<i);
	}
	srand(seed);

	PIO pio = pio0;
	uint offset = pio_add_program(pio, &led_pio_program);
	uint sm = pio_claim_unused_sm(pio, true);
	led_pio_program_init(pio, sm, offset);

	bool STATUS_LED_STATE = false;

	
	gpio_init(PIN_STATUS_LED);
	gpio_set_dir(PIN_STATUS_LED, GPIO_OUT);


	dma_init(pio, sm);

	absolute_time_t start_time;
	absolute_time_t end_time;
	uint32_t i = 0;
	while (true) {

		start_time = get_absolute_time();
		update_frame_matrix(&frame_matrix, i);
		end_time = get_absolute_time();
		if(i > 1000){
			printf("matrix_time: %" PRId64 "  ", absolute_time_diff_us(start_time, end_time));
		}



		start_time = get_absolute_time();
		render_led_frame(pio, sm, &frame_matrix);
		end_time = get_absolute_time();

		uint64_t render_time = absolute_time_diff_us(start_time, end_time);
		if(render_time < MIN_FRAME_TIME){
			sleep_us(MIN_FRAME_TIME-render_time);
		}

		if(i > 1000){
			printf("frame_time: %" PRId64 "  sleep_time: %" PRId64 "  ", render_time, MIN_FRAME_TIME-render_time);
		}

		start_time = get_absolute_time();
		update_dim();
		end_time = get_absolute_time();
		if(i > 1000){
			printf("dim_time: %" PRId64 "  ", absolute_time_diff_us(start_time, end_time));
		}

		start_time = get_absolute_time();
		gpio_put(PIN_STATUS_LED, STATUS_LED_STATE);
		STATUS_LED_STATE = !(STATUS_LED_STATE);
		end_time = get_absolute_time();
		if(i > 1000){
			printf("status_time: %" PRId64 "\n", absolute_time_diff_us(start_time, end_time));
		}
		i++;
		if(i > 1001){
			i = 1;
		}
	
	}

	return;
}




void __attribute__((noinline, section(".time_critical"))) update_frame_matrix_row_by_row_y(frame_matrix_t *frame_matrix, uint32_t i){
	if(i%20 == 0){
		frame_matrix->pixel[0][0] += 1;
		if(frame_matrix->pixel[0][0] > 16){
			frame_matrix->pixel[0][0] = 1;
		}
	}
	for(int32_t x = 1; x <= 16; x++){
		for(int32_t y = 1; y<= 16; y++){
			if(y == frame_matrix->pixel[0][0]){
				frame_matrix->pixel[x][y] = 0x01;
			}else{
				frame_matrix->pixel[x][y] = 0x00;
			}
		}
	}
}

void __attribute__((noinline, section(".time_critical"))) update_frame_matrix_row_by_row_x(frame_matrix_t *frame_matrix, uint32_t i){
        if(i%20 == 0){
                frame_matrix->pixel[0][0] += 1;
                if(frame_matrix->pixel[0][0] > 16){ 
                        frame_matrix->pixel[0][0] = 1;
                }
        }
        for(int32_t x = 1; x <= 16; x++){
                for(int32_t y = 1; y<= 16; y++){
                        if(x == frame_matrix->pixel[0][0]){
                                frame_matrix->pixel[x][y] = 0x01;
                        }else{
                                frame_matrix->pixel[x][y] = 0x00;
                        }
                }
        }
}

void __attribute__((noinline, section(".time_critical"))) update_frame_matrix_scan_x_y(frame_matrix_t *frame_matrix, uint32_t i){
        if(i%5 == 0){
                frame_matrix->pixel[0][0] += 1;
                if(frame_matrix->pixel[0][0] > 16){
                	frame_matrix->pixel[0][0] = 1;
			frame_matrix->pixel[0][1]++;
			if(frame_matrix->pixel[0][1] > 16){
				frame_matrix->pixel[0][1] = 1;
			}
		}
	}
	for(int32_t x = 1; x <= 16; x++){
		for(int32_t y = 1; y<= 16; y++){
			if(x == frame_matrix->pixel[0][0] && y == frame_matrix->pixel[0][1]){
				frame_matrix->pixel[x][y] = 0x01;
			}else{
			        frame_matrix->pixel[x][y] = 0x00;
			}
		}
	}
}

void __attribute__((noinline, section(".time_critical"))) update_frame_matrix_random_per_pixel(frame_matrix_t *frame_matrix, uint32_t i){
	if(i % 20 != 0){
		return;
	}

	for( int32_t x = 1; x <= 16; x++){
		for( int32_t y = 1; y <= 16; y++){
			int32_t val = rand();
			uint8_t r = (val & 0x0000ff) >> 3;
			uint8_t b = (val & 0x00ff00) >> 8+3;
			uint8_t g = (val & 0xff0000) >> 16+3;
			
			uint8_t test = rand()/(RAND_MAX>>3); //0-7

			if(test == 0){
				r = 0;
			}else if (test == 1){
				b = 0;
			}else if (test == 2){
				g = 0;
			}else if (test == 3){
				r = 0;
				b = 0;
			}else if (test == 4){
				r = 0;
				g = 0;
			}else if( test == 5){
				b = 0;
				g = 0;
			}else if ( test == 6){
                                r = 0;
                                b = 0;
                                g = 0;
                        }

			

			frame_matrix->pixel[x][y] = r | (b << 8) | (g << 16);
		}
	}
}

void __attribute__((noinline, section(".time_critical"))) update_frame_matrix_random_per_pixeasdl(frame_matrix_t *frame_matrix, uint32_t i){
	for( int32_t x = 1; x <= 16; x++){
	        for( int32_t y = 1; y <= 16; y++){
		}
	}
}


void __attribute__((noinline, section(".time_critical"))) update_frame_matrix(frame_matrix_t *frame_matrix, uint32_t i){
	int32_t val;
	uint8_t test;
	if(i == 0 || dim == DIM_MIN){
		val = rand();
		frame_matrix->pixel[0][0] = val;
		test = rand()/(RAND_MAX>>3); //0-7
		frame_matrix->pixel[0][1] = test;
	}else{
		val = frame_matrix->pixel[0][0];
		test = frame_matrix->pixel[0][1];
	}

		uint8_t r = (val & 0x0000ff) >> 3;
                uint8_t b = (val & 0x00ff00) >> 8+3;
                uint8_t g = (val & 0xff0000) >> 16+3;

                        if(test == 0){
                                r = 0; 
                        }else if (test == 1){
                                b = 0; 
                        }else if (test == 2){
                                g = 0; 
                        }else if (test == 3){
                                r = 0;
                                b = 0; 
                        }else if (test == 4){
                                r = 0;
                                g = 0; 
                        }else if( test == 5){
                                b = 0;
                                g = 0;
                        }
                        
        for( int32_t x = 1; x <= 16; x++){
                for( int32_t y = 1; y <= 16; y++){
			int32_t chance = dim*(x*y)/DIM_MAX;
                        frame_matrix->pixel[x][y] = (chance*r/256) | ((chance*b/256) << 8) | ((chance*g/256) << 16);
                }
        }
}

void __attribute__((noinline, section(".time_critical"))) update_dim(){
	if(dim_polarity){
        	dim_counter++;
        }else{
        	dim_counter--;
        }

	//dim_counter exists to support variable dim rates w/ variable framerate
	dim = dim_counter/DIM_COUNTER_DIVISOR;

      	if(dim > DIM_MAX){
		dim_polarity = !dim_polarity;
		dim = DIM_MAX;
		dim_counter = DIM_MAX*DIM_COUNTER_DIVISOR;
	}else if(dim < DIM_MIN){
		dim = DIM_MIN;
		dim_counter = DIM_MIN*DIM_COUNTER_DIVISOR;
        	dim_polarity = !dim_polarity;
        }

	return;
}
/*
void __attribute__((noinline, section(".time_critical"))) randomize_color(){
        color_red += rand()/ (RAND_MAX >> 4);
        color_green += rand()/(RAND_MAX >> 4);
        color_blue += rand()/ (RAND_MAX >> 4);

	printf("Random Color: R: %d, G: %d, B: %d\n", color_red, color_green, color_blue);

        if(color_red > 64){
        	color_red = 0;
        }

        if(color_green > 64){
        	color_green = 0;
        }

        if(color_blue > 64){
        	color_blue = 0;
        }

	return;
}
*/

void __attribute__((noinline, section(".time_critical"))) render_led_frame(PIO pio, uint sm, frame_matrix_t *frame_matrix){
        for(int32_t pwm = 0; pwm < 64; pwm++){
                for(int32_t y = 1; y <= 16; y++){
			for(int32_t x = 16; x >= 1; x-=2){
			uint32_t buf;
			int32_t chance = 0;
			int32_t chance1 = 0;
			buf = DEFAULT_PIXEL_PACKET |
                                ( (chance  + ((frame_matrix->pixel[x][y]   & 0x0000FF) >> 0 ) > pwm) << BYTE0 + RED   ) | ( (chance  + ((frame_matrix->pixel[x][y]   & 0x0000FF) >> 0 ) > pwm) << BYTE1 + RED   ) |
                                ( (chance1 + ((frame_matrix->pixel[x-1][y] & 0x0000FF) >> 0 ) > pwm) << BYTE2 + RED   ) | ( (chance1 + ((frame_matrix->pixel[x-1][y] & 0x0000FF) >> 0 ) > pwm) << BYTE3 + RED   ) |
                                ( (chance  + ((frame_matrix->pixel[x][y]   & 0x00FF00) >> 8 ) > pwm) << BYTE0 + BLUE  ) | ( (chance  + ((frame_matrix->pixel[x][y]   & 0x00FF00) >> 8 ) > pwm) << BYTE1 + BLUE  ) |
                                ( (chance1 + ((frame_matrix->pixel[x-1][y] & 0x00FF00) >> 8 ) > pwm) << BYTE2 + BLUE  ) | ( (chance1 + ((frame_matrix->pixel[x-1][y] & 0x00FF00) >> 8 ) > pwm) << BYTE3 + BLUE  ) |
                                ( (chance  + ((frame_matrix->pixel[x][y]   & 0xFF0000) >> 16) > pwm) << BYTE0 + GREEN ) | ( (chance  + ((frame_matrix->pixel[x][y]   & 0xFF0000) >> 16) > pwm) << BYTE1 + GREEN ) |
                                ( (chance1 + ((frame_matrix->pixel[x-1][y] & 0xFF0000) >> 16) > pwm) << BYTE2 + GREEN ) | ( (chance1 + ((frame_matrix->pixel[x-1][y] & 0xFF0000) >> 16) > pwm) << BYTE3 + GREEN ) |
                                ( (x!=y) << BYTE0 + ROW   ) | ((x!=y) << BYTE1 + ROW   ) |
                                ( (x-1!=y) << BYTE2 + ROW   ) | ((x-1!=y) << BYTE3 + ROW   );
                        pio_sm_put_blocking(pio, sm, buf);
			pio_sm_put_blocking(pio, sm, DEFAULT_FLUSH_PACKET);
			}
                	pio_sm_put_blocking(pio, sm, DEFAULT_LATCH_PACKET);
			pio_sm_put_blocking(pio, sm, DEFAULT_FLUSH_PACKET);

                }

        }

	return;
}
