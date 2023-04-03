/**
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Modified to use a hardware interpolator, R.P.Bellis/20230402
 */

#include <stdio.h>
#include <math.h>

#include "pico/stdlib.h"
#include "pico/audio_i2s.h"
#include "hardware/interp.h"

#define SINE_WAVE_TABLE_LEN 2048
#define SAMPLES_PER_BUFFER 256

static int16_t sine_wave_table[SINE_WAVE_TABLE_LEN];

struct audio_buffer_pool *init_audio() {

    static audio_format_t audio_format = {
            .format = AUDIO_BUFFER_FORMAT_PCM_S16,
            .sample_freq = 44100,
            .channel_count = 1,
    };

    static struct audio_buffer_format producer_format = {
            .format = &audio_format,
            .sample_stride = 2
    };

    struct audio_buffer_pool *producer_pool = audio_new_producer_pool(&producer_format, 3,
                                                                      SAMPLES_PER_BUFFER);
    bool __unused ok;
    const struct audio_format *output_format;

    struct audio_i2s_config config = {
            .data_pin = PICO_AUDIO_I2S_DATA_PIN,
            .clock_pin_base = PICO_AUDIO_I2S_CLOCK_PIN_BASE,
            .dma_channel = 0,
            .pio_sm = 0,
    };

    output_format = audio_i2s_setup(&audio_format, &config);
    if (!output_format) {
        panic("PicoAudio: Unable to open audio device.\n");
    }

    ok = audio_i2s_connect(producer_pool);
    assert(ok);
    audio_i2s_set_enabled(true);
    return producer_pool;
}

int main() {

    stdio_init_all();

    for (int i = 0; i < SINE_WAVE_TABLE_LEN; i++) {
        sine_wave_table[i] = 32767 * cosf(i * 2 * (float) (M_PI / SINE_WAVE_TABLE_LEN));
    }

    struct audio_buffer_pool *ap = init_audio();
    uint32_t step = 0x200000;
    uint32_t pos_max = 0x10000 * SINE_WAVE_TABLE_LEN;

    // set up the interpolator so that its output is a
    // pointer directly into the 16-bit wave table.
    interp_config cfg = interp_default_config();
    interp_config_set_shift(&cfg, 15);        // 16 bit sub-position, but the table entries are 2 bytes
    interp_config_set_mask(&cfg, 1, 11);      // mask the output to the range 2 .. (2 << 11)
    interp_config_set_add_raw(&cfg, true);    // add the raw step size, not the shifted and masked
    interp_set_config(interp0, 0, &cfg);

    interp0->accum[0] = 0;
    interp0->base[2] = (uint32_t)sine_wave_table;
    interp0->base[0] = step;

    uint vol = 32;
    while (true) {

        int c = getchar_timeout_us(0);
        if (c >= 0) {
            if (c == '-' && vol) vol -= 4;
            if ((c == '=' || c == '+') && vol < 255) vol += 4;
            if (c == '[' && step > 0x10000) step -= 0x10000;
            if (c == ']' && step < (SINE_WAVE_TABLE_LEN / 16) * 0x20000) step += 0x10000;
            if (c == 'q') break;
            printf("vol = %d, step = %d      \r", vol, step >> 16);

            // update the interpolator step size
            interp0->base[0] = step;
        }

        struct audio_buffer *buffer = take_audio_buffer(ap, true);
        int16_t* samples = (int16_t *) buffer->buffer->bytes;

        for (uint i = 0; i < buffer->max_sample_count; ) {
            // get the sample pointer from the interpolator
            int16_t* p  = (int16_t*)interp0->pop[2];
            samples[i++] = (vol * (*p)) >> 8u;
        }

        buffer->sample_count = buffer->max_sample_count;
        give_audio_buffer(ap, buffer);
    }
    puts("\n");
    return 0;
}
