/* SPDX-License-Identifier: GPL-3.0-or-later
 * ps5_beeper.h - PS5 hardware beeper library
 *
 * Confirmed sceKernelIccSetBuzzer argument map (2026-05-24):
 *   0  silent      ICC set_buzzer(0), no audible output, ret=0
 *   1  single      ICC set_buzzer(1), one short beep, ret=0
 *   2  error       ICC set_buzzer(2), error tone pattern (multi-beep), ret=0
 *   3  long        ICC set_buzzer(3), one long sustained beep, ret=0
 *
 * Confirmed sceKernelIccSetBuzzerDimSetting (volume) map (2026-05-24):
 *   0  High        full volume (default)
 *   1  Medium      mid volume
 *   2  Low         low volume
 *   3  INVALID     ret=0x80020005
 *
 * Confirmed sceKernelIccSetBuzzerOffSetting (mute) map (2026-05-24):
 *   0  unmuted     beep plays normally
 *   1  muted       no audible output (confirmed via eject button test)
 *   2  muted       also muted (same result as 1)
 *
 * Usage:
 *   #include "ps5_beeper.h"
 *   // add ps5_beeper.c to your build
 *
 *   ps5_beeper_single();          // one short beep
 *   ps5_beeper_error();           // error tone pattern
 *   ps5_beeper_set_volume(PS5_BEEPER_VOLUME_HIGH);   // restore full volume
 *   ps5_beeper_set_mute(PS5_BEEPER_MUTE_ON);         // silence all beeps
 *   ps5_beeper_set_mute(PS5_BEEPER_MUTE_OFF);        // restore beeps
 *
 * Requirements:
 *   - PS5 payload SDK (prospero toolchain)
 *   - Link: -lSceUserService -lpthread
 *   - sceKernelIccSetBuzzer must be in SDK stubs (it is)
 *   - sceKernelIccSetBuzzerDimSetting and sceKernelIccSetBuzzerOffSetting are
 *     resolved at runtime (absent from SDK stubs); no extra link flags needed
 */

#ifndef PS5_BEEPER_H
#define PS5_BEEPER_H

/* Beep type constants — all four values confirmed 2026-05-24 */
#define PS5_BEEPER_VALUE_SILENT  0
#define PS5_BEEPER_VALUE_SINGLE  1
#define PS5_BEEPER_VALUE_ERROR   2
#define PS5_BEEPER_VALUE_LONG    3

/* Volume level constants for ps5_beeper_set_volume() — confirmed 2026-05-24 */
#define PS5_BEEPER_VOLUME_HIGH   0
#define PS5_BEEPER_VOLUME_MED    1
#define PS5_BEEPER_VOLUME_LOW    2

/* Mute state constants for ps5_beeper_set_mute() — confirmed 2026-05-24 */
#define PS5_BEEPER_MUTE_OFF      0   /* unmuted: beeps play normally */
#define PS5_BEEPER_MUTE_ON       1   /* muted: all beeps silenced    */

/* LED brightness constants for ps5_beeper_set_led() — confirmed 2026-05-24
 * sceKernelIccSetDynamicLedDimSetting: 0=Bright 1=Medium 2=Dim */
#define PS5_BEEPER_LED_BRIGHT    0
#define PS5_BEEPER_LED_MEDIUM    1
#define PS5_BEEPER_LED_DIM       2

int ps5_beeper_silent(void);
int ps5_beeper_single(void);
int ps5_beeper_error(void);
int ps5_beeper_long(void);
int ps5_beeper_raw(int value);

/* Set beeper volume. Use PS5_BEEPER_VOLUME_* constants.
 * ICC state persists until changed or console power-cycles.
 * Returns 0 on success, non-zero on error. */
int ps5_beeper_set_volume(int level);

/* Set beeper mute state. Use PS5_BEEPER_MUTE_ON / PS5_BEEPER_MUTE_OFF.
 * ICC state persists until changed or console power-cycles.
 * Returns 0 on success, non-zero on error. */
int ps5_beeper_set_mute(int mute);

/* Set LED brightness. Use PS5_BEEPER_LED_* constants.
 * ICC state persists until changed or console power-cycles.
 * Returns 0 on success, non-zero on error. */
int ps5_beeper_set_led(int level);

#endif /* PS5_BEEPER_H */
