/* Skan80 - German 80-channel CB frequency plan (kHz).
 *
 * Source: Bundesnetzagentur, CB Allgemeinzuteilung Vfg. 21/2021.
 * Index 0..79 corresponds to channel 1..80.
 * Scan order follows channel numbers, not monotonically increasing RF.
 *
 * Copyright 2026; Apache-2.0; based on the channel mapping in
 * Kriss80/SP6HAX's CBSkaner application.
 */
#ifndef SKAN80_CHANNELS_H
#define SKAN80_CHANNELS_H
#include <stdint.h>

static inline uint32_t cb_channel_khz(uint8_t index)
{
    if (index >= 40u)
        return 26565u + 10u * (uint32_t)(index - 40u);

    /* Channels 23, 24 and 25 are historically out of frequency order. */
    if (index == 22u)
        index = 24u;
    else if (index == 23u || index == 24u)
        index = (uint8_t)(index - 1u);

    uint32_t f = 26965u + 10u * (uint32_t)index;
    f += 10u * ((index >= 3u) + (index >= 7u) + (index >= 11u) +
                (index >= 15u) + (index >= 19u));
    return f;
}
#endif
