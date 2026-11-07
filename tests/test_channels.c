/* Host-side test: compile with cc -std=c11 -Wall -Wextra -Werror.
 * Official frequency table (Bundesnetzagentur Vfg. 21/2021).
 */
#include <stdint.h>
#include <stdio.h>
#include "cb_channels.h"

static const uint32_t ch1to40[40] = {
    26965,26975,26985,27005,27015,27025,27035,27055,27065,27075,
    27085,27105,27115,27125,27135,27155,27165,27175,27185,27205,
    27215,27225,27255,27235,27245,27265,27275,27285,27295,27305,
    27315,27325,27335,27345,27355,27365,27375,27385,27395,27405
};

int main(void)
{
    for (unsigned ch=0; ch<80u; ++ch) {
        const uint32_t expected = ch<40u ? ch1to40[ch] : (26565u+10u*(ch-40u));
        const uint32_t actual = cb_channel_khz((uint8_t)ch);
        if (actual != expected) {
            fprintf(stderr,"CH%02u: expected %u kHz, got %u kHz\n",ch+1,expected,actual);
            return 1;
        }
        if (actual<26565u || actual>27405u) return 2;
        for (unsigned prev=0; prev<ch; ++prev)
            if (actual==cb_channel_khz((uint8_t)prev)) {
                fprintf(stderr,"duplicate channels %u/%u\n",prev+1,ch+1);
                return 3;
            }
    }
    puts("PASS: all 80 CB channels match German assignment; unique; 26.565..27.405 MHz");
    return 0;
}
