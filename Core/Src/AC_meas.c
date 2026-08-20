/*
 * AC_meas.c
 *
 *  Created on: May 02, 2026
 *      Author: Andrew Jian
 */

#include "AC_meas.h"
#include "menu.h"
#include <math.h>

#define M_PI 3.14159265358979323846f

// --- TIME CONSTANTS ---
#define RMS_SAMPLE_TIME 40 // ms
#define PHASE_SAMPLE_TIME 60 // ms

// --- HARDWARE CONSTANTS ---
#define VREF 3.28f
#define ADC_MAX_VAL 4095.0f

// --- SQUELCH / DEADBAND ---
#define NOISE_FLOOR_ADC_COUNTS 25.0f

// --- HARDWARE MULTIPLIERS ---
#define AC_VOLT_HW_MULTIPLIER 8.6274f
#define AC_VOLT_CAL_FACTOR    1.0000f
#define AC_CURR_HIGH_MULTIPLIER (9.483f * 2.0f)
#define AC_CURR_LOW_MULTIPLIER  (1.333f * 2.0f)

#define AC_INVALID_VALUE -1.0f

// ==========================================
// RAW ADC DEBUG BUFFERS
// ==========================================
volatile uint32_t debug_live_raw_v = 0;    // Single bouncing value
volatile uint32_t debug_v_snapshot[200];   // Holds one full cycle of the AC Wave!


// --- DYNAMIC STATE ---
static uint8_t current_range_is_high = 1; // 1 is high current range
// Track physical pin state to prevent unnecessary delays
static uint8_t actual_hardware_range = 255;

uint8_t AC_Meas_GetRangeIsHigh(void) {
    return current_range_is_high;
}

void AC_Meas_Init(void) {
    HAL_GPIO_WritePin(GPIOC, RANGE_SELECT_Pin, GPIO_PIN_RESET);
    actual_hardware_range = 1;
    current_range_is_high = 1; // defult at high current range mode
}

// ==============================================================================
// CURRENT & VOLTAGE (RMS)
// ==============================================================================

static uint32_t Measure_AC_Voltage(ADC_HandleTypeDef* hadc_volt) {
    hadc_volt->Instance->SQR1 &= ~(ADC_SQR1_L);

    uint64_t sum_x = 0;
    uint64_t sum_x2 = 0;
    uint32_t count = 0;
    uint32_t start_time = HAL_GetTick();

    // Removed HAL_ADC_Stop to allow maximum polling speed
    while ((HAL_GetTick() - start_time) < RMS_SAMPLE_TIME) {
        __HAL_ADC_CLEAR_FLAG(hadc_volt, ADC_FLAG_EOC | ADC_FLAG_EOS | ADC_FLAG_OVR);

        HAL_ADC_Start(hadc_volt);
        if (HAL_ADC_PollForConversion(hadc_volt, 1) == HAL_OK) {
            uint32_t val = HAL_ADC_GetValue(hadc_volt);
            sum_x += val;
            sum_x2 += (val * val);
            count++;
        }
    }
    HAL_ADC_Stop(hadc_volt); // Only stop AFTER the entire batch is done

    if (count == 0) return 2048;

    float mean = (float)sum_x / (float)count;
    float mean_square = (float)sum_x2 / (float)count;
    float variance = mean_square - (mean * mean);
    if (variance < 0.0f) variance = 0.0f;

    float rms_adc = sqrtf(variance);

    if (rms_adc < NOISE_FLOOR_ADC_COUNTS) {
        live_ac_v = -999.0f;
        live_ac_pp_v = -999.0f;
        return (uint32_t)mean;
    }

    float vrms_pin = rms_adc * (VREF / ADC_MAX_VAL);
    live_ac_v = vrms_pin * AC_VOLT_HW_MULTIPLIER * AC_VOLT_CAL_FACTOR * step_down_ratio;
    live_ac_pp_v = live_ac_v * 2.828427f;

    if (live_ac_v < 0.25f) live_ac_v = -999.0f;

    return (uint32_t)mean;
}

static uint32_t Measure_AC_Current(ADC_HandleTypeDef* hadc_curr) {
    ADC_ChannelConfTypeDef sConfig = {0};
    sConfig.Rank = ADC_REGULAR_RANK_1;
    sConfig.SamplingTime = ADC_SAMPLETIME_247CYCLES_5;

    // 1. Map the internal ADC multiplexer to the correct pin
    if (current_range_is_high) {
        sConfig.Channel = ADC_CHANNEL_1; // PC0 (High Current)
    } else {
        sConfig.Channel = ADC_CHANNEL_2; // PC1 (Low Current)
    }

    HAL_ADC_ConfigChannel(hadc_curr, &sConfig);
    hadc_curr->Instance->SQR1 &= ~(ADC_SQR1_L);

    // 2. Clear any lingering charge on the sample-and-hold capacitor
    for(int i = 0; i < 3; i++) {
        __HAL_ADC_CLEAR_FLAG(hadc_curr, ADC_FLAG_EOC | ADC_FLAG_EOS | ADC_FLAG_OVR);
        HAL_ADC_Start(hadc_curr);
        HAL_ADC_PollForConversion(hadc_curr, 1);
        HAL_ADC_GetValue(hadc_curr);
    }

    // 3. High-Speed 40ms Sample Accumulation (Using 64-bit integers)
    uint64_t sum_x = 0;
    uint64_t sum_x2 = 0;
    uint32_t count = 0;
    uint32_t start_time = HAL_GetTick();

    uint32_t snap_idx = 0; // <-- ADD THIS

    uint64_t minADC = 4095;
    uint64_t maxADC = 0;

    while ((HAL_GetTick() - start_time) < RMS_SAMPLE_TIME) {
        __HAL_ADC_CLEAR_FLAG(hadc_curr, ADC_FLAG_EOC | ADC_FLAG_EOS | ADC_FLAG_OVR);

        HAL_ADC_Start(hadc_curr);
        if (HAL_ADC_PollForConversion(hadc_curr, 1) == HAL_OK) {
            uint32_t val = HAL_ADC_GetValue(hadc_curr);

            if (val < minADC) {minADC = val;}
            if (val > maxADC) {maxADC = val;}

            debug_live_raw_v = val; // Track the single fastest value
            if (snap_idx < 200) {
                debug_v_snapshot[snap_idx] = val; // Save array of the wave
                snap_idx++;
            }

            sum_x += val;
            sum_x2 += (val * val);
            count++;
        }
    }
    HAL_ADC_Stop(hadc_curr);

    if (count == 0) return 2048;

    // 4. Pure Integer Variance Arithmetic
    uint64_t mean_sq_sum = (sum_x * sum_x) / count;
    uint64_t variance_int = 0;

    if (sum_x2 > mean_sq_sum) {
        variance_int = (sum_x2 - mean_sq_sum) / count; // center in middle not 0
    }

    float rms_adc = (float)sqrt((double)variance_int);
    uint32_t d_mean = (uint32_t)(sum_x / count);

    if (rms_adc < NOISE_FLOOR_ADC_COUNTS) {
        live_ac_i = -999.0f;
        live_ac_pp_i = -999.0f;
    } else {
       // Explicitly scale the raw counts across the full ADC dynamic range
       float vrms_pin = (rms_adc * VREF) / ADC_MAX_VAL;

       if (current_range_is_high) {
           live_ac_i = vrms_pin * AC_CURR_HIGH_MULTIPLIER;
        } else {
           live_ac_i = vrms_pin * AC_CURR_LOW_MULTIPLIER;
        }
        live_ac_pp_i = live_ac_i * 2.828427f;
    }


    // 5. Dual-Range Hysteresis Engine with Hardware Pin Integration
    if (current_range_is_high) {
        // If current drops below 1.0A, drop down to Low Range channel
        if (live_ac_i != -999.0f && live_ac_i < 1.0f) {
            current_range_is_high = 0;
            // COMMAND PHYSICAL PCB TO SWITCH TO LOW-CURRENT CIRCUIT
            HAL_GPIO_WritePin(GPIOC, RANGE_SELECT_Pin, GPIO_PIN_SET);
            actual_hardware_range = 0;
        }
    } else {
        // If current exceeds 1.1A, switch up to High Range channel
        if (live_ac_i > 1.1f) {
            current_range_is_high = 1;
            // COMMAND PHYSICAL PCB TO SWITCH TO HIGH-CURRENT CIRCUIT
            HAL_GPIO_WritePin(GPIOC, RANGE_SELECT_Pin, GPIO_PIN_RESET);
            actual_hardware_range = 1;
        }
    }

    // 6. Final Outputs
    live_ac_pp_i = live_ac_i * 2.828427f;
    if (live_ac_i < 0.025f) live_ac_i = -999.0f;

    return d_mean;
}

// ==============================================================================
// PHASE AND POWER CALCULATIONS
// ==============================================================================

static void Measure_AC_Phase_And_Freq(ADC_HandleTypeDef* hadc_volt, ADC_HandleTypeDef* hadc_curr, uint32_t v_center, uint32_t i_center) {

    // skip calculation if there is no valid AC signal to measure
    if (live_ac_v == -999.0f || live_ac_i == -999.0f) {
        live_ac_freq = -999.0f;
        live_ac_phase = -999.0f;
        return;
    }

    // Force single conversion mode by clearing sequence length bits
    hadc_volt->Instance->SQR1 &= ~(ADC_SQR1_L);
    hadc_curr->Instance->SQR1 &= ~(ADC_SQR1_L);

    // Variables to track the loop iteration (time) when zero-crossings occur
    uint32_t v_cross_1 = 0, v_cross_2 = 0, i_cross_1 = 0;
    uint8_t v_cross_count = 0, i_cross_count = 0;

    // (prevents noise from triggering false crossings)
    uint8_t v_ready_to_cross = 0;
    uint8_t i_ready_to_cross = 0;

    uint32_t loop_count = 0;
    uint32_t start_time = HAL_GetTick();

    // --- HIGH SPEED POLLING LOOP ---
    while ((HAL_GetTick() - start_time) < PHASE_SAMPLE_TIME) {
        __HAL_ADC_CLEAR_FLAG(hadc_volt, ADC_FLAG_EOC | ADC_FLAG_EOS | ADC_FLAG_OVR);
        __HAL_ADC_CLEAR_FLAG(hadc_curr, ADC_FLAG_EOC | ADC_FLAG_EOS | ADC_FLAG_OVR);

        HAL_ADC_Start(hadc_volt);
        HAL_ADC_Start(hadc_curr);

        // Sample Voltage
        HAL_ADC_PollForConversion(hadc_volt, 1);
        uint32_t curr_v = HAL_ADC_GetValue(hadc_volt);

        // Sample Current
        HAL_ADC_PollForConversion(hadc_curr, 1);
        uint32_t curr_i = HAL_ADC_GetValue(hadc_curr);

        // --- Voltage zero-crossing detector (negative -> positive) ---
        // Arm the detector only when the wave dips significantly below the center (30-counts)
        if (curr_v < (v_center - 30)) v_ready_to_cross = 1;

        // Trigger crossing when it surges significantly above the center
        if (v_ready_to_cross && curr_v > (v_center + 30)) {
            v_ready_to_cross = 0;
            if (v_cross_count == 0) {
                v_cross_1 = loop_count; // Record first V crossing
                v_cross_count++;
            } else if (v_cross_count == 1) {
                // Ignore high-frequency noise spikes; ensure crossings are far apart
                if ((loop_count - v_cross_1) > 100) {
                    v_cross_2 = loop_count; // Record second V crossing (1 full period)
                    v_cross_count++;
                }
            }
        }

        // --- Current zero-crossing detector (negative to positive) ---
        if (curr_i < (i_center - 30)) i_ready_to_cross = 1;

        if (i_ready_to_cross && curr_i > (i_center + 30)) {
            i_ready_to_cross = 0;
            if (i_cross_count == 0) {
                i_cross_1 = loop_count; // Record first I crossing
                i_cross_count++;
            }
        }

        loop_count++;
    }

    // Stop the ADCs ONLY after the timing-critical loop finishes
    HAL_ADC_Stop(hadc_volt);
    HAL_ADC_Stop(hadc_curr);

    uint32_t total_time_ms = HAL_GetTick() - start_time;

    // --- Freq & Phase calculations ---
    // at least one full Voltage period and one Current crossing
    if (v_cross_count >= 2 && i_cross_count >= 1) {

        // Calculate dynamic polling speed to convert loop counts into real time
        float time_per_loop_ms = (float)total_time_ms / (float)loop_count;
        float wave_period_ms = (float)(v_cross_2 - v_cross_1) * time_per_loop_ms;
        live_ac_freq = 1000.0f / wave_period_ms; // f = 1/T

        // Phase offset measured in loop iterations
        int32_t sample_diff = (int32_t)i_cross_1 - (int32_t)v_cross_1;
        int32_t period_samples = (int32_t)(v_cross_2 - v_cross_1);

        // Wrap the phase difference to the nearest half-period (-180 to +180 deg)
        while (sample_diff > (period_samples / 2)) sample_diff -= period_samples;
        while (sample_diff < -(period_samples / 2)) sample_diff += period_samples;

        // Convert the sample difference into degrees
        live_ac_phase = ((float)sample_diff / (float)period_samples) * 360.0f;
        live_ac_phase = fabs(live_ac_phase); // For Power Factor, phase polarity doesn't matter

    } else {
        // Failed to capture enough wave data (when freq is too low or...)
        live_ac_freq = -999.0f;
        live_ac_phase = -999.0f;
    }
}

static void Calculate_AC_Power(void) {
    if (live_ac_v == -999.0f || live_ac_i == -999.0f || live_ac_phase == -999.0f) {
        live_ac_app = -999.0f;
        live_ac_real = -999.0f;
        live_ac_react = -999.0f;
        live_ac_pf = -999.0f;
    } else {
        live_ac_app = live_ac_v * live_ac_i;
        live_ac_pf = cos(live_ac_phase * (M_PI / 180.0f));
        live_ac_real = live_ac_app * live_ac_pf;
        live_ac_react = sqrt(fabs((live_ac_app * live_ac_app) - (live_ac_real * live_ac_real)));
    }
}

void AC_Meas_Update(ADC_HandleTypeDef* hadc_ac_volt, ADC_HandleTypeDef* hadc_ac_curr) {
	uint32_t v_center = Measure_AC_Voltage(hadc_ac_volt);
    Blink_Measurement_LED();

    uint32_t i_center = Measure_AC_Current(hadc_ac_curr);
    Blink_Measurement_LED();

    Measure_AC_Phase_And_Freq(hadc_ac_volt, hadc_ac_curr, v_center, i_center);
    Blink_Measurement_LED();

    Calculate_AC_Power();
}

