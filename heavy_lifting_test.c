//#define USE_STACK_GUARDS 0

#define PIN_I2C_SDA 8
#define PIN_I2C_SCL 9

#define PICO_STDOUT_MUTEX 0


#include "heavy_lifting_test.h"
#include "tinyusb_multitool.h"
#include "pico/stdio_uart.h"
#include "kxtj3.h"
#include "pico/runtime.h"


int32_t dim = 0;
int32_t dim_counter = 0;
bool dim_polarity = false;
uint32_t program_counter = 0;

frame_matrix_t frame_matrix;

//enough space for 2 sequential frames, then we alternate between them on IRQ.
uint32_t * dma_address[2];

PIO pio;
uint sm;

static bool STATUS_LED_STATE = false;
static bool AM_I_DEAD = false;

void flash_pending_cb(){
	_DBG("Received flash pending callback, attempting to kill core1");
	multicore_fifo_push_blocking(0xdead);
	_DBG("Successfully sent core1 death signal, checking for response");

	busy_wait_ms(100);
	uint32_t ret = multicore_fifo_pop_blocking(); // Return when we receive a response
	_DBG("Received response from core1 0x%x", ret);
	return;
}


int main() {
	multicore_lockout_victim_init(); // Used for flashing
	//stdio_init_all();
	multicore_reset_core1(); // Need reset for hot-restarts to work right
	multicore_launch_core1(core1_entry);

        tumt_usb_init(flash_pending_cb);
        tumt_uart_bridge_uart0_init(115200);
        tumt_uart_bridge_uart1_init(921600);
        tumt_uart_bridge_pin_init();

	uint32_t i = 0;
	while(true){
		if(i == 1000){
			_DBG("core0 Alive! %" PRId64 "", get_absolute_time());
			i = 0;
		}

		//tumt_periodic_task();
		sleep_ms(1);
		i++;
	}

	return 0;
}



const led_matrix_mode_t led_matrix_modes[] = {
	{
		.ptr = update_frame_matrix_row_by_row_y, 
		.function_name = "row_by_row_y", 
		.pretty_name = "Row by Row (Y)",
	},
	{
		.ptr = update_frame_matrix_row_by_row_x,
		.function_name = "row_by_row_x",
	       	.pretty_name = "Row by Row (X)",
	},
	{
		.ptr = update_frame_matrix_scan_x_y,
		.function_name = "scan_x_y",
	   	.pretty_name = "Scan X by Y",
	},
	{
		.ptr = update_frame_matrix_random_per_pixel,
		.function_name = "random_per_pixel",
	       	.pretty_name = "Random per Pixel",
	},
        {
		.ptr = update_frame_matrix_random_corner_breath,
		.function_name = "random_corner_breath", 
		.pretty_name = "Random Corner Breath",
	},
	{
		.ptr = update_frame_matrix_random_brightness_test,
		.function_name = "random_brightness_test",
		.pretty_name = "Random Brightness Test",
	},
	{
		.ptr = update_frame_matrix_kxtj3,
		.function_name = "kxtj3",
		.pretty_name = "kxtj3",
	}
};




void (*update_frame_matrix)(frame_matrix_t*, uint32_t) = (led_matrix_modes[4].ptr);



void dma_init(PIO pio, uint sm){

	dma_address[DMA_CHANNEL_A] = malloc(BYTES_PER_FRAME);
	dma_address[DMA_CHANNEL_B] = malloc(BYTES_PER_FRAME);
	memset(dma_address[DMA_CHANNEL_A], 0x0, BYTES_PER_FRAME);
	memset(dma_address[DMA_CHANNEL_B], 0x0, BYTES_PER_FRAME);

	do_render_frame(DMA_CHANNEL_A);
	do_render_frame(DMA_CHANNEL_B);

	dma_claim_mask(DMA_CHANNELS_MASK);

	dma_channel_config channel_config_a = dma_channel_get_default_config(DMA_CHANNEL_A);
	channel_config_set_dreq(&channel_config_a, pio_get_dreq(pio, sm, true));
	channel_config_set_transfer_data_size(&channel_config_a, DMA_SIZE_32);
	//channel_config_set_chain_to(&channel_config_a, DMA_CHANNEL_B);
	channel_config_set_irq_quiet(&channel_config_a, false);
	channel_config_set_ring(&channel_config_a, true, 1);
	dma_channel_configure(DMA_CHANNEL_A,
        		&channel_config_a,
			&pio->txf[sm],
			dma_address[DMA_CHANNEL_A],
			BYTES_PER_FRAME/4, // 4 bytes per packet, 8 packets per X, 2 packets per row,  16 Y, 64 PWM
			false
			);

	dma_channel_config channel_config_b = dma_channel_get_default_config(DMA_CHANNEL_B);
	channel_config_set_dreq(&channel_config_b, pio_get_dreq(pio, sm, true));
	channel_config_set_transfer_data_size(&channel_config_b, DMA_SIZE_32);
	//channel_config_set_chain_to(&channel_config_b, DMA_CHANNEL_A);
	channel_config_set_irq_quiet(&channel_config_b, false);
	channel_config_set_ring(&channel_config_b, true, 1);
	dma_channel_configure(DMA_CHANNEL_B,
                          &channel_config_b,
                          &pio->txf[sm],
                          dma_address[DMA_CHANNEL_B],
                          BYTES_PER_FRAME/4,
                          false
			);

	irq_set_exclusive_handler(DMA_IRQ_0, dma_complete_handler_a);
	irq_set_exclusive_handler(DMA_IRQ_1, dma_complete_handler_b);

	dma_channel_set_irq0_enabled(DMA_CHANNEL_A, true);
	dma_channel_set_irq1_enabled(DMA_CHANNEL_B, true);

	irq_set_enabled(DMA_IRQ_0, true);
	irq_set_enabled(DMA_IRQ_1, true);

	dma_start_channel_mask(DMA_CHANNELS_MASK);
}

void __isr dma_complete_handler_a() {
	// clear IRQ
	dma_hw->ints0 |= DMA_CHANNEL_A_MASK;

	if(check_should_i_die())
		return;
	do_render_frame(DMA_CHANNEL_A);	
	dma_channel_set_read_addr(DMA_CHANNEL_A, dma_address[DMA_CHANNEL_A], true);
}
void __isr dma_complete_handler_b(){
	dma_hw->ints1 |= DMA_CHANNEL_B_MASK;
	do_render_frame(DMA_CHANNEL_B);
	dma_channel_set_read_addr(DMA_CHANNEL_B, dma_address[DMA_CHANNEL_B], true);
}

void __attribute__((noinline, section(".time_critical"))) core1_entry(){
	int32_t seed = 0;
	for( uint8_t i = 0; i <32; i++){
		seed |= (rosc_hw->randombit<<i);
	}
	srand(seed);

	pio = pio0;
	uint offset = pio_add_program(pio, &led_pio_program);
	sm = pio_claim_unused_sm(pio, true);
	led_pio_program_init(pio, sm, offset);

	gpio_init(PIN_STATUS_LED);
	gpio_set_dir(PIN_STATUS_LED, GPIO_OUT);

	dma_init(pio, sm);

	while (true) {
		sleep_ms(100);
		_DBG("core1 IDLE heartbeat %" PRId64 "", get_absolute_time());
		if(check_should_i_die()){
			printf("\n\n------------------------\n");
			printf("------ CORE1 EXIT ------\n");
			printf("------------------------\n\n\n");
			fflush(stdout);
			sleep_ms(100);
			return;
		}
	}
}

bool __attribute__((section(".time_critical"))) check_should_i_die(){
	if(multicore_fifo_rvalid()){
		_DBG("core1: rvalid on fifo, checking for message");
		uint32_t val = multicore_fifo_pop_blocking();
		_DBG("core1: Got message [0x%x] on fifo, killing core1", val);
		//If we got something on the fifo, it's time to die.
		irq_set_enabled(DMA_IRQ_0, false);
		irq_set_enabled(DMA_IRQ_1, false);
		pio_sm_set_enabled(pio, sm, false);
		pio_sm_unclaim(pio, sm);
		multicore_fifo_push_blocking(0xDEADBEEF);
		_DBG("core1: DMA/PIO have stopped");
		AM_I_DEAD = true;
	}

	return AM_I_DEAD;
}

void __attribute__((section(".time_critical"))) do_render_frame(uint8_t dma_channel_output){	
	//absolute_time_t start_time = get_absolute_time();

	//start_time = get_absolute_time();
	update_frame_matrix(&frame_matrix, program_counter);
	//end_time = get_absolute_time();
	//if(program_counter == 950)
	//	printf("matrix_time: %" PRId64 "  ", absolute_time_diff_us(start_time, end_time));
	


//	start_time = get_absolute_time();
	render_led_frame(&frame_matrix, dma_channel_output);
//	end_time = get_absolute_time();

//	uint64_t render_time = absolute_time_diff_us(start_time, end_time);
	//if(render_time < MIN_FRAME_TIME){
	//	sleep_us(MIN_FRAME_TIME-render_time);
	//}

	//if(program_counter == 950){
//		printf("frame_time: %" PRId64 "  sleep_time: %" PRId64 "  ", render_time, MIN_FRAME_TIME-render_time);
//	}

//	start_time = get_absolute_time();
	update_dim();
//	end_time = get_absolute_time();
//	if(program_counter == 950){
//		printf("dim_time: %" PRId64 "  ", absolute_time_diff_us(start_time, end_time));
//	}

//	start_time = get_absolute_time();
	gpio_put(PIN_STATUS_LED, STATUS_LED_STATE);
	STATUS_LED_STATE = !(STATUS_LED_STATE);
//	end_time = get_absolute_time();
	//if(i == 950){
	//	printf("status_time: %" PRId64 "\n", absolute_time_diff_us(start_time, end_time));
	//}
	
	program_counter++;
	if(program_counter > 1001){
		//printf("do_render_frame loop at 1k - %" PRId64 "\n", get_absolute_time() );
		program_counter = 1;
	}
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


void __attribute__((noinline, section(".time_critical"))) update_frame_matrix_random_corner_breath(frame_matrix_t *frame_matrix, uint32_t i){
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
void __attribute__((noinline, section(".time_critical"))) update_frame_matrix_random_brightness_test(frame_matrix_t *frame_matrix, uint32_t i){
	int32_t val;
	if(dim == DIM_MIN){
		val = rand();
		frame_matrix->pixel[0][0] = val;
	}else{
		val = frame_matrix->pixel[0][0];
	}

	uint8_t r = (val & 0x0000ff) >> 3;
        uint8_t b = (val & 0x00ff00) >> 8+3;
        uint8_t g = (val & 0xff0000) >> 16+3;

	for( int32_t x = 1; x <= 16; x++){
		for( int32_t y = 1; y<= 16; y++){
			//uint32_t r_final = (r-dim)/4;
			//uint32_t b_final = (b-dim)/4;
			//uint32_t g_final = (g-dim)/4;
			//if(r_final > 64) r_final = 0;
			//if(b_final > 64) b_final = 0;
			//if(g_final > 64) g_final = 0;

			//frame_matrix->pixel[x][y] = (r_final) | (b_final << 8) | (g_final << 16);
			frame_matrix->pixel[x][y] = (0x0) | (0x0 << 8) | (dim << 16);
		}

	}
}

void __attribute__((noinline, section(".time_critical"))) update_frame_matrix_kxtj3(frame_matrix_t *frame_matrix, uint32_t i){
	int32_t val;
	for( int32_t x = 1; x <= 16; x++){
		for( int32_t y = 1; y <= 16; y++){
			int32_t chance = dim*(x*y)/DIM_MAX;
			//frame_matrix->pixel[x][y] = (chance*r/256) | ((chance*b/256) << 8) | ((chance*g/256) << 16);
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

#define PIXEL_PACKET(...) \
	*buf = DEFAULT_PIXEL_PACKET | \
		( (((frame_matrix->pixel[x][y]   & 0x0000FF) >> 0 ) > pwm) << BYTE0 + RED   ) | ( (((frame_matrix->pixel[x][y]   & 0x0000FF) >> 0 ) > pwm) << BYTE1 + RED   ) | \
		( (((frame_matrix->pixel[x-1][y] & 0x0000FF) >> 0 ) > pwm) << BYTE2 + RED   ) | ( (((frame_matrix->pixel[x-1][y] & 0x0000FF) >> 0 ) > pwm) << BYTE3 + RED   ) | \
		( (((frame_matrix->pixel[x][y]   & 0x00FF00) >> 8 ) > pwm) << BYTE0 + BLUE  ) | ( (((frame_matrix->pixel[x][y]   & 0x00FF00) >> 8 ) > pwm) << BYTE1 + BLUE  ) | \
		( (((frame_matrix->pixel[x-1][y] & 0x00FF00) >> 8 ) > pwm) << BYTE2 + BLUE  ) | ( (((frame_matrix->pixel[x-1][y] & 0x00FF00) >> 8 ) > pwm) << BYTE3 + BLUE  ) | \
		( (((frame_matrix->pixel[x][y]   & 0xFF0000) >> 16) > pwm) << BYTE0 + GREEN ) | ( (((frame_matrix->pixel[x][y]   & 0xFF0000) >> 16) > pwm) << BYTE1 + GREEN ) | \
		( (((frame_matrix->pixel[x-1][y] & 0xFF0000) >> 16) > pwm) << BYTE2 + GREEN ) | ( (((frame_matrix->pixel[x-1][y] & 0xFF0000) >> 16) > pwm) << BYTE3 + GREEN ) | \
		( (x!=y) << BYTE0 + ROW   ) | ((x!=y) << BYTE1 + ROW   ) | \
		( (x-1!=y) << BYTE2 + ROW   ) | ((x-1!=y) << BYTE3 + ROW   ); \
		buf++;
		//pio->txf[sm] = buf;
		//pio_sm_put_blocking(pio, sm, buf);

void __attribute__((section(".time_critical"))) render_led_frame(frame_matrix_t register *frame_matrix, uint8_t dma_channel){
	uint32_t register *buf = dma_address[dma_channel];
        for(uint8_t register pwm = 0; pwm < PWM_MAX; pwm++){
                for(uint8_t register y = 1; y <= X_PER_Y; y++){
			//for(int32_t x = 16; x > 1; x-=2){
			uint8_t register x;

			x = 16;
			PIXEL_PACKET();

			x = 14;
			PIXEL_PACKET();

			x = 12;
			PIXEL_PACKET();

			x = 10;
			PIXEL_PACKET();

			x = 8;
			PIXEL_PACKET();

			x = 6;
			PIXEL_PACKET();
			
			x = 4;
			PIXEL_PACKET();

			x = 2;
			PIXEL_PACKET();




			//}
			*buf = DEFAULT_LATCH_PACKET;
			buf++;
			*buf = DEFAULT_FLUSH_PACKET;
			buf++;
                }
        }
	return;
}
