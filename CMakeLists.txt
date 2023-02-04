cmake_minimum_required(VERSION 3.18)

include(pico_sdk_import.cmake)

project(pipicogpsclock C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
pico_sdk_init()

add_executable(pipicogpsclock
	pipicogpsclock.c
)

pico_add_extra_outputs(pipicogpsclock)

target_link_libraries(pipicogpsclock pico_stdlib pico_stdlib hardware_spi)
