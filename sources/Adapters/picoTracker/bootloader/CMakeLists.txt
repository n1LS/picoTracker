add_executable(picoTrackerBootloader
  bootloader_main.cpp
  uf2_parser.cpp
  flash_writer.cpp
  menu.cpp
  path_utils.cpp
  slot_boot.cpp
  bootloader_chargfx.c
  trace_wrap.cpp
  ${PROJECT_SOURCE_DIR}/Adapters/picoTracker/display/ili9341.c
  ${PROJECT_SOURCE_DIR}/Adapters/picoTracker/platform/platform.cpp
  ${PROJECT_SOURCE_DIR}/Adapters/picoTracker/mutex/picoTrackerMutex.cpp
  ${PROJECT_SOURCE_DIR}/Adapters/picoTracker/system/input.cpp
)

target_link_libraries(picoTrackerBootloader PUBLIC
    # Keep bootloader deps minimal and self-contained.
    platform_sdcard

    # Foundation
    foundation_services
    foundation
    
    # External and SDK libraries
    pico_sdk_bundle
)

target_include_directories(picoTrackerBootloader PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${PROJECT_SOURCE_DIR})

target_compile_options(picoTrackerBootloader PRIVATE
  $<$<COMPILE_LANGUAGE:ASM>:-Os>
  $<$<COMPILE_LANGUAGE:C>:-Os>
  $<$<COMPILE_LANGUAGE:CXX>:-Os>
  $<$<COMPILE_LANGUAGE:C>:-fno-unwind-tables>
  $<$<COMPILE_LANGUAGE:C>:-fno-asynchronous-unwind-tables>
  $<$<COMPILE_LANGUAGE:CXX>:-fno-unwind-tables>
  $<$<COMPILE_LANGUAGE:CXX>:-fno-asynchronous-unwind-tables>
  $<$<COMPILE_LANGUAGE:CXX>:-fno-threadsafe-statics>
  $<$<COMPILE_LANGUAGE:CXX>:-fno-use-cxa-atexit>
  $<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions>
  $<$<COMPILE_LANGUAGE:CXX>:-fno-rtti>
)

target_link_options(picoTrackerBootloader PRIVATE
  -Wl,--wrap=_ZN5Trace5DebugEPKcz
  -Wl,--wrap=_ZN5Trace3LogEPKcS1_z
)

target_compile_definitions(picoTrackerBootloader PUBLIC
  PICO_FLASH_SIZE_BYTES=16*1024*1024
  PICO_XOSC_STARTUP_DELAY_MULTIPLIER=64
  PICO_PRINTF_SUPPORT_FLOAT=0
  PICO_PRINTF_SUPPORT_EXPONENTIAL=0
)

pico_enable_stdio_usb(picoTrackerBootloader 0)
pico_enable_stdio_uart(picoTrackerBootloader 0)

# Create map/bin/hex/uf2 file etc.
pico_add_extra_outputs(picoTrackerBootloader)
