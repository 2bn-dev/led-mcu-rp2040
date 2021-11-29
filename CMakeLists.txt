cmake_minimum_required(VERSION 3.13)

# initialize the SDK based on PICO_SDK_PATH
# note: this must happen before project()
include(pico_sdk_import.cmake)

project(led_mcu_rp2040)

# initialize the Raspberry Pi Pico SDK
pico_sdk_init()

# rest of your project

add_executable(hello_world
    hello_world.c
)

add_executable(heavy_lifting_test
	heavy_lifting_test.c
)

pico_generate_pio_header(
	heavy_lifting_test 
	${CMAKE_CURRENT_LIST_DIR}/pinout/2bn-mcumz16163-a.pio
	)
pico_generate_pio_header(
	heavy_lifting_test
	${CMAKE_CURRENT_LIST_DIR}/led_dma.pio
	)

pico_generate_pio_header(
	hello_world 
	${CMAKE_CURRENT_LIST_DIR}/pinout/2bn-mcumz16163-a.pio
	)


# Add pico_stdlib library which aggregates commonly used features
target_link_libraries(
	hello_world 
	pico_stdlib
	hardware_pio
	pico_time
	)
target_link_libraries(
	heavy_lifting_test
       	pico_stdlib 
	hardware_pio
	pico_time
	pico_platform
	pico_multicore
	hardware_dma
	hardware_irq
	)

pico_enable_stdio_usb(hello_world 1)
pico_enable_stdio_uart(hello_world 0)
pico_enable_stdio_usb(heavy_lifting_test 1)
pico_enable_stdio_uart(heavy_lifting_test 0)



# create map/bin/hex/uf2 file in addition to ELF.
pico_add_extra_outputs(hello_world)
pico_add_extra_outputs(heavy_lifting_test)



