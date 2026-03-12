/*
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2024 xiphonics, inc.
 *
 * This file is part of the picoTracker firmware
 */

#include "Adapters/picoTracker/platform/platform.h"
#include "Adapters/picoTracker/system/picoTrackerSystem.h"
#include "Adapters/picoTracker/usb/msd_mode.h"
#include "Application/Application.h"
#include "bsp/board.h"
#include "hardware/clocks.h"
#include "hardware/pll.h"
#include "pico/stdlib.h"
#include "pico/time.h"
#include "tusb.h"
#include <System/Console/Trace.h>

#include "../system/input.h"

// this prevents a really annoying linker warning due to newlib and >gcc13
// ref:
// https://github.com/raspberrypi/pico-sdk/issues/1768#issuecomment-2649260970
#ifdef __cplusplus
extern "C" {
int _getentropy(void *buffer, size_t length) { return -1; }
}
#endif
// ================

// Defined in msd_scsi.cpp
extern "C" bool msd_sd_init(void);

int main(int argc, char *argv[]) {

  // Initialise microcontroller specific hardware
  board_init();

  // Check if MSD mode was requested before previous reboot
  if (msd_mode_requested()) {
    g_msd_mode = true;

    // Need platform_init for GPIO (display, SD card, input buttons)
    platform_init();

    // Initialize TinyUSB with MSD descriptors active
    tusb_init();

    // Initialize SD card for MSD access
    msd_sd_init();

    // Enter MSD mode loop - this never returns (reboots on keypress)
    msd_mode_run();
  }

  // Normal mode: Initialise TinyUSB
  tusb_init();

  // Do remaining pT init, this needs to be done *after* above hardware and
  // tinyusb subsystem init
  platform_init();

  // Check for EDIT key hold on boot to force load untitled project
  {
    uint16_t keys = scanKeys();
    if (keys & KEY_EDIT) { // Check for INPUT_EDIT (bit 6)
      forceLoadUntitledProject = true;
    }
  }

  // Make sure we get ETL logs
  Trace::RegisterEtlErrorHandler();

  picoTrackerSystem::Boot(argc, argv);

  GUICreateWindowParams params;
  params.title = "picoTracker";

  Application::GetInstance()->Init(params);

  picoTrackerSystem::MainLoop();
  // WE NEVER GET HERE

  picoTrackerSystem::Shutdown();
}
