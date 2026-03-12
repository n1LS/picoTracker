/*
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2024 xiphonics, inc.
 *
 * This file is part of the picoTracker firmware
 *
 * USB Mass Storage Device mode - allows the SD card to be accessed
 * as a USB mass storage device from a host computer.
 */

#ifndef _MSD_MODE_H_
#define _MSD_MODE_H_

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Magic value written to watchdog scratch[5] to request MSD mode on reboot
#define MSD_MAGIC 0x4D534400 // "MSD\0"

// Check if MSD mode was requested (via watchdog scratch register)
// Clears the magic value so we don't get stuck in MSD mode
bool msd_mode_requested(void);

// Run the MSD mode main loop - displays status on screen, services USB,
// and reboots back to normal mode on any keypress.
// This function never returns (it reboots the device).
void msd_mode_run(void);

// Global flag indicating we are in MSD mode - used by USB descriptor
// callbacks to return MSC-only descriptors instead of CDC+MIDI
extern bool g_msd_mode;

#ifdef __cplusplus
}
#endif

#endif /* _MSD_MODE_H_ */
