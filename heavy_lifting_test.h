#include <stdio.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "2bn-mcumz16163-a.pio.h"
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




/************************
 * unused
 */

// Some DMA code copy pasted from https://github.com/raspberrypi/pico-examples/blob/master/pio/ws2812/ws2812_parallel.c
// bit plane content dma channel
#define DMA_CHANNEL 0

// chain channel for configuring main dma channel to output from disjoint 8 word fragments of memory
#define DMA_CB_CHANNEL 1
#define DMA_CHANNEL_MASK (1u << DMA_CHANNEL)
#define DMA_CB_CHANNEL_MASK (1u << DMA_CB_CHANNEL)
#define DMA_CHANNELS_MASK (DMA_CHANNEL_MASK | DMA_CB_CHANNEL_MASK)


typedef struct {
	const void * ptr;
	const char* function_name;
	const char* pretty_name;
	bool debug; // May want to ignore debug type modes in the future.
} led_matrix_mode_t;


void dma_init(PIO pio, uint sm);
void __isr dma_complete_handler();
void core1_entry();
void __attribute__((noinline, section(".time_critical"))) update_frame_matrix_row_by_row_y(frame_matrix_t *frame_matrix, uint32_t i);
void __attribute__((noinline, section(".time_critical"))) update_frame_matrix_row_by_row_x(frame_matrix_t *frame_matrix, uint32_t i);
void __attribute__((noinline, section(".time_critical"))) update_frame_matrix_scan_x_y(frame_matrix_t *frame_matrix, uint32_t i);
void __attribute__((noinline, section(".time_critical"))) update_frame_matrix_random_per_pixel(frame_matrix_t *frame_matrix, uint32_t i);
void __attribute__((noinline, section(".time_critical"))) update_frame_matrix_random_corner_breath(frame_matrix_t *frame_matrix, uint32_t i);
void __attribute__((noinline, section(".time_critical"))) update_dim();
void __attribute__((noinline, section(".time_critical"))) randomize_color();
void __attribute__((noinline, section(".time_critical"))) render_led_frame(PIO pio, uint sm, frame_matrix_t *frame_matrix);
void flash_pending_cb();
