cmake_minimum_required(VERSION 3.14)

project(Pico-HuBASIC C CXX ASM)

# C++17 support as requested in specification
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Option to switch between embedded Pico build and Host unit test build
option(BUILD_TESTS "Build host unit tests instead of Pico UF2" OFF)

if(BUILD_TESTS)
    message(STATUS "🚀 Building for HOST tests (Google Test will be downloaded)")
    
    # 1. Fetch Google Test
    include(FetchContent)
    FetchContent_Declare(
      googletest
      URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip
    )
    # Prevent overriding the parent project's compiler/linker settings on Windows
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)

    # 2. Build the core components without Pico's stdio or HAL dependencies
    add_library(core_lib
        src/lexer.cpp
        src/parser.cpp
    )
    target_include_directories(core_lib PUBLIC src)

    # 3. Create the basic_tests executable linking against GTest
    add_executable(basic_tests
        tests/test_lexer.cpp
        tests/test_parser.cpp
        tests/test_input_end_stop.cpp
        tests/test_graphics.cpp
        tests/test_gpio_brightness.cpp
        tests/test_advanced_graphics.cpp
        tests/test_extended_core.cpp
        tests/test_file_ops.cpp
        tests/test_builtin_functions_ext.cpp
        tests/test_advanced_features.cpp
        tests/test_sound_logic.cpp
        tests/mock_hal_display.cpp
        src/hal_gpio.cpp
        src/hal_sound.cpp
    )
    target_include_directories(basic_tests PRIVATE src tests)
    target_link_libraries(basic_tests PRIVATE core_lib GTest::gtest_main)

    include(GoogleTest)
    gtest_discover_tests(basic_tests)

else()
    message(STATUS "🍓 Building for Pico RP2350 target")
    
    include(pico_sdk_import.cmake OPTIONAL)

    if (COMMAND pico_sdk_init)
        pico_sdk_init()
    else()
        message(WARNING "pico_sdk_import.cmake is not found. Please ensure Raspberry Pi Pico SDK is configured properly.")
    endif()

    # Define the RP2350 target
    set(PICO_BOARD waveshare_rp2350_touch_lcd_2_8 CACHE STRING "Board type")

    add_executable(pico_hubasic
        src/main.cpp
        src/hal_display.cpp
        src/repl.cpp
        src/lexer.cpp
        src/parser.cpp
    )

    if (COMMAND pico_set_target_board)
        pico_set_target_board(pico_hubasic ${PICO_BOARD})
    endif()

    if (TARGET pico_stdlib)
        target_link_libraries(pico_hubasic
            pico_stdlib
            hardware_spi
            hardware_i2c
            hardware_pwm
        )
        
        # Use USB for standard I/O (Terminal connection)
        pico_enable_stdio_usb(pico_hubasic 1)
        pico_enable_stdio_uart(pico_hubasic 0)
        
        # Generate UF2 for RP2350
        pico_add_extra_outputs(pico_hubasic)
    endif()
endif()
