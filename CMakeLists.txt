cmake_minimum_required(VERSION 3.13)

# initialize the SDK based on PICO_SDK_PATH
# note: this must happen before project()
include(pico_sdk_import.cmake)

project(led_mcu_rp2040)

# initialize the Raspberry Pi Pico SDK
pico_sdk_init()

# rest of your project

include(../tinyusb-multitool/CMakeLists.txt)
include(../kxtj3/CMakeLists.txt)


add_executable(led_matrix_controller
	main.c
)

pico_generate_pio_header(
	led_matrix_controller
	${CMAKE_CURRENT_LIST_DIR}/pinout/2bn-mcumz16163-a.pio
	)
pico_generate_pio_header(
	led_matrix_controller
	${CMAKE_CURRENT_LIST_DIR}/led_dma.pio
	)


# Add pico_stdlib library which aggregates commonly used features
target_link_libraries(
	led_matrix_controller
       	pico_stdlib 
	hardware_pio
	pico_time
	pico_platform
	pico_multicore
	hardware_dma
	hardware_irq
	tinyusb_multitool
	kxtj3
	pico_stdio_uart
	)

pico_enable_stdio_usb(led_matrix_controller 0)
pico_enable_stdio_uart(led_matrix_controller 0)



# create map/bin/hex/uf2 file in addition to ELF.
pico_add_extra_outputs(led_matrix_controller)


