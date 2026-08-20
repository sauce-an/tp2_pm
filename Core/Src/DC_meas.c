/*
 * DC_meas.c
 */

#include "DC_meas.h"
#include "menu.h"
#include <math.h>

// --- HARDWARE CONSTANTS ---
#define VREF 3.28f
#define ADC_MAX_VAL 4095.0f

// --- V2 PCB HARDWARE MULTIPLIERS ---
// The V2 PCB physically steps the voltage down by ~1/2 for safety.
// We reverse that here so the calibration factors remain pure.
#define V2_BOARD_DIVIDER_RATIO 2.010f

#define HALL_SENSITIVITY 0.200f
#define VOLT_HARDWARE_MULTIPLIER -6.25f

// --- PURE CALIBRATION CONSTANTS ---
#define CURR_CAL_FACTOR_LOW   0.995f
#define CURR_CAL_FACTOR_HIGH  0.995f
#define CURR_CAL_BREAKPOINT   1.0f

#define VOLT_CAL_FACTOR       1.010f

// --- DYNAMIC STATE ---
float actual_hall_offset = 1.65f;
float actual_volt_offset = 1.65f;

// ==============================================================================
// THE SEQUENCE-ABORT ADC READER
// ==============================================================================

static uint32_t Read_DC_ADC(ADC_HandleTypeDef* hadc_dc, uint32_t channel, int samples) {

    // map desired channel to Rank 1 using compliant HAL code
    ADC_ChannelConfTypeDef sConfig = {0};
    sConfig.Channel = channel;
    sConfig.Rank = ADC_REGULAR_RANK_1;
    sConfig.SamplingTime = ADC_SAMPLETIME_640CYCLES_5; // Max time to charge capacitor
    sConfig.SingleDiff = ADC_SINGLE_ENDED; // Mandatory for STM32L4!
    sConfig.OffsetNumber = ADC_OFFSET_NONE;
    sConfig.Offset = 0;

    if (HAL_ADC_ConfigChannel(hadc_dc, &sConfig) != HAL_OK) {
        return 2048; // Failsafe
    }

    uint32_t total_adc = 0;
    uint32_t valid_samples = 0;

    for(int i = 0; i < samples; i++) {
        // Clear flags to prevent sticky error locks
        __HAL_ADC_CLEAR_FLAG(hadc_dc, ADC_FLAG_EOC | ADC_FLAG_EOS | ADC_FLAG_OVR);

        // Starts the sequence at Rank 1
        HAL_ADC_Start(hadc_dc);

        // Generous 10ms timeout to wait for Rank 1
        if (HAL_ADC_PollForConversion(hadc_dc, 10) == HAL_OK) {
            total_adc += HAL_ADC_GetValue(hadc_dc); // Grabs Rank 1
            valid_samples++;
        }

        // CRITICAL ABORT:
        // CubeMX wants to scan Rank 2. We violently abort the ADC right here!
        // This throws away Rank 2 and forces the internal sequencer to reset back
        // to Rank 1 for the next loop iteration. No interleaving!
        HAL_ADC_Stop(hadc_dc);
    }

    if (valid_samples == 0) return 2048; // Failsafe

    return total_adc / valid_samples;
}

// ==============================================================================
// INITIALIZATION & MEASUREMENTS
// ==============================================================================

void DC_Meas_Init(ADC_HandleTypeDef* hadc_dc) {
    // auto-tare current sensor (Channel 6)
    uint32_t avg_hall = Read_DC_ADC(hadc_dc, ADC_CHANNEL_6, 100);
    actual_hall_offset = ((float)avg_hall * VREF) / ADC_MAX_VAL;

    // auto-tare voltage sensor (Channel 5)
    uint32_t avg_volt = Read_DC_ADC(hadc_dc, ADC_CHANNEL_5, 100);
    actual_volt_offset = ((float)avg_volt * VREF) / ADC_MAX_VAL;
}

static void Measure_DC_Current(ADC_HandleTypeDef* hadc_dc) {
    uint32_t avg_adc = Read_DC_ADC(hadc_dc, ADC_CHANNEL_6, 128);

    float voltage_read = ((float)avg_adc * VREF) / ADC_MAX_VAL;
    float raw_current = (voltage_read - actual_hall_offset) / HALL_SENSITIVITY;

    //raw_current *= V2_BOARD_DIVIDER_RATIO;

    if (fabs(raw_current) > CURR_CAL_BREAKPOINT) {
        live_dc_i = raw_current * CURR_CAL_FACTOR_HIGH;
    } else {
        live_dc_i = raw_current * CURR_CAL_FACTOR_LOW;
    }

    // deadband filter
    if (live_dc_i > -0.04f && live_dc_i < 0.04f) {
        live_dc_i = 0.00f;
    }
}

void Measure_DC_Voltage(ADC_HandleTypeDef* hadc_dc) {
    uint32_t avg_adc = Read_DC_ADC(hadc_dc, ADC_CHANNEL_5, 128);

    float voltage_read = ((float)avg_adc * VREF) / ADC_MAX_VAL;
    float raw_voltage = (voltage_read - actual_volt_offset) * VOLT_HARDWARE_MULTIPLIER;

    // Apply the V2 Board physical step-down multiplier
    //raw_voltage *= V2_BOARD_DIVIDER_RATIO;

    live_dc_v = raw_voltage * VOLT_CAL_FACTOR * step_down_ratio;

    if (live_dc_v > -0.05f && live_dc_v < 0.05f) {
        live_dc_v = 0.00f;
    }

}

static void Calculate_DC_Power(void) {
    live_dc_w = live_dc_v * live_dc_i;
}


void Blink_Measurement_LED(void) {
    // 1. Turn LED ON
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_4, GPIO_PIN_SET);

    // 2. Wait exactly 50 milliseconds
    HAL_Delay(1);

    // 3. Turn LED OFF
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_4, GPIO_PIN_RESET);
}

void DC_Meas_Update(ADC_HandleTypeDef* hadc_dc) {
    Measure_DC_Current(hadc_dc);
    Blink_Measurement_LED();

    Measure_DC_Voltage(hadc_dc);
    Blink_Measurement_LED();

    Calculate_DC_Power();
}

