/*
 * SPDX-FileCopyrightText: 2026 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

enum PATTERN_PERFORM_STATE {
    PATTERN_PERFORM_START = 1,
    PATTERN_PERFORM_INTERRUPT,
    PATTERN_PERFORM_RESUME,
    PATTERN_PERFORM_BUSY,
    PATTERN_PERFORM_END,
    INVALID_PATTERN_STATUS,
};

extern "C" {
int si_vibra_init(uint32_t* deviceType);
int si_vibra_on(unsigned int timeout_ms);
int si_vibra_off();
int si_vibra_setAmplitude(uint8_t amplitude);
int si_vibra_setting_f0(int f0);
int si_vibra_set_drc_mode(int mode);
int si_vibra_dynamic_scale(uint8_t scale);
int si_vibra_update_parameter();
int wave_vib_lib_init();

void si_vibra_looper_start();
int32_t si_vibra_looper_post(const int32_t* pattern, int32_t patternLen, int32_t intervalMs,
                             int32_t loopNum, int32_t amplitude, int32_t freq);
bool si_vibra_looper_performParam(int32_t intervalMs, int32_t amplitude, int32_t freq);
bool si_vibra_looper_stopPerformHe(void);

int32_t si_vibra_looper_on(uint32_t time_out);
int32_t si_vibra_looper_prebaked_effect(uint32_t effect_id, int32_t strength);
int32_t si_vibra_looper_envelope(const int32_t* envelope_data, uint32_t data_len,
                                 bool fastFlag);
int32_t si_vibra_looper_rtp(int32_t fd);
}
