add_library(platform_usb
  usb_utils.cpp 
  usb_descriptors.c
  msd_mode.c
  msd_scsi.cpp
)

target_link_libraries(platform_usb PUBLIC
  pico_sdk_bundle
  platform_sdcard
  platform_display
)

target_include_directories(platform_usb PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
  
include_directories(${PROJECT_SOURCE_DIR})
