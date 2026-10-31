/* Copyright 2026 Kriss80 Elektryczny SP6HAX
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 *     Unless required by applicable law or agreed to in writing, software
 *     distributed under the License is distributed on an "AS IS" BASIS,
 *     WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *     See the License for the specific language governing permissions and
 *     limitations under the License.
 */

/*
 * CB Skaner - listens to the 40 CB channels (27 MHz) on the UV-K1 with
 * F4HWN Labs. Receive only: the app calls no transmit service.
 *
 * Channels are computed from the CEPT/FCC plan instead of a table: each one is
 * 10 kHz above the previous, with five gaps (3-4, 7-8, 11-12, 15-16, 19-20)
 * and the historical 23 / 24 / 25 swap. Raster "0" is the same plan shifted
 * by -5 kHz (channel 19 = 27.180 MHz).
 *
 * Tuning and AM/FM go through the BK4819 registers as the firmware does
 * (radio.c, spectrum3d); 27 MHz is below the 280 MHz VHF/UHF switch, so the
 * VHF path is selected. The loader restores the VFO on exit.
 *
 * Features: scanning that stops on a signal, a skip list, the last active
 * channel, an RSSI squelch with hysteresis, monitor.
 *
 * Keys: UP/DOWN channel · * scan · MENU AM/FM · 1/7 squelch -/+ ·
 *       3 raster 0/5 · 5 skip channel in scan · 8 last active channel ·
 *       9 channel 19 · 0 monitor · EXIT quit.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "app_api.h"

#ifndef APP_VERSION
#define APP_VERSION "dev"     /* build.sh passes the release version */
#endif
#define SIGNATURE "v" APP_VERSION " SP6HAX"

/* ---- receiver registers ---- */
enum {
    BK_CTRL    = 0x30,
    BK_DEMOD   = 0x31,          /* bit 0: AM demodulation (UV-K1)      */
    BK_GPIO    = 0x33,
    BK_FREQ_L  = 0x38,
    BK_FREQ_H  = 0x39,
    BK_AFC     = 0x73,          /* bit 4: AFC disabled                 */
};
#define LNA_UHF        0x08u
#define LNA_VHF        0x04u

/* ---- parameters ---- */
#define CHANNELS       40u
#define CHANNEL_19     18u      /* zero-based index                    */
#define DWELL_MS       150u     /* time per channel while scanning     */
#define RESUME_MS      2000u    /* silence after a signal before scanning on */
#define SETTLE_MS      40u      /* PLL and RSSI after retuning         */
#define SQ_MIN         2        /* squelch: dB above the measured noise floor */
#define SQ_MAX         40
#define SQ_DEFAULT     10
#define SQ_STEP        2
#define SQ_HYSTERESIS  3
#define BAR_MIN        (-126)   /* signal bar: -126 .. -50 dBm         */
#define BAR_END        104      /* last bar pixel; "(1-7)" follows it  */
#define OPEN_SAMPLES   4        /* 40 ms above the threshold to open   */
#define CLOSE_SAMPLES  15       /* 150 ms below it to close            */
#define CFG_MAGIC      0x5A
#define NONE           0xFFu

enum state { IDLE, RECEIVING, WAITING };

/* Saved settings: exactly this order, byte by byte. */
typedef struct {
    uint8_t magic;
    uint8_t channel;
    uint8_t flags;              /* bit 0: AM, bit 1: raster "0"        */
    int8_t  sq;                 /* dB above the noise floor            */
    uint8_t skipped[5];         /* 40 bits, a set bit = skipped channel */
} settings_t;

#define FLAG_AM     0x01u
#define FLAG_ZERO   0x02u

static struct {
    settings_t cfg;
    uint8_t state, last, key;
    bool scan, monitor, running, redraw, audio;
    uint8_t count;              /* samples in a row past the threshold */
    int16_t rssi;               /* averaged, dBm                       */
    int32_t rssiQ4;             /* running average, dBm x 16           */
    int32_t floorQ4;            /* noise floor, dBm x 16               */
    bool floorSet;
    uint16_t ctrl;
    uint32_t tunedAt, signalAt, pressedAt, drawnAt;
    const app_api_t *api;
} s;

#define API (s.api)

void *memset(void *d, int c, size_t n)
{
    uint8_t *p = d;
    while (n--)
        *p++ = (uint8_t)c;
    return d;
}

/* Divisions through the resident helpers (API level 2), no libgcc copy. */
uint64_t __aeabi_idivmod(int32_t n, int32_t d){ return s.api->idivmod(n, d); }
uint64_t __aeabi_idiv(int32_t n, int32_t d) __attribute__((alias("__aeabi_idivmod")));
uint64_t __aeabi_uidivmod(uint32_t n, uint32_t d){ return s.api->uidivmod(n, d); }
uint64_t __aeabi_uidiv(uint32_t n, uint32_t d) __attribute__((alias("__aeabi_uidivmod")));

/* ---- channel plan ---- */

static uint32_t channelKhz(uint8_t ch)
{
    /* swapped channels 23-25: 27.255 / 27.235 / 27.245 */
    if (ch == 22u)                   ch = 24u;
    else if (ch == 23u || ch == 24u) ch = (uint8_t)(ch - 1u);

    uint32_t f = 26965u + 10u * ch;
    f += 10u * ((ch >= 3u) + (ch >= 7u) + (ch >= 11u) + (ch >= 15u) + (ch >= 19u));
    if (s.cfg.flags & FLAG_ZERO)
        f -= 5u;
    return f;
}

static bool isSkipped(uint8_t ch)
{
    return (s.cfg.skipped[ch >> 3] >> (ch & 7u)) & 1u;
}

/* ---- radio ---- */

/* Speaker on/off: the amplifier and the receiver's AF output together. */
static void setAudio(bool on)
{
    if (on == s.audio)
        return;
    s.audio = on;
    API->set_af(on ? APP_AF_FM : APP_AF_MUTE);   /* AM too: radio.c keeps AF in FM */
    API->audio_path(on);
}

static void tune(void)
{
    const uint32_t f = channelKhz(s.cfg.channel) * 100u;   /* 10 Hz units */
    void (*const write)(uint8_t, uint16_t) = API->bk_write;

    write(BK_FREQ_L, (uint16_t)f);
    write(BK_FREQ_H, (uint16_t)(f >> 16));
    write(BK_CTRL, 0);
    write(BK_CTRL, s.ctrl);
    setAudio(s.monitor);
    s.tunedAt = API->ticks_ms();
    s.state = IDLE;
    s.redraw = true;
}

static void setModulation(void)
{
    void (*const write)(uint8_t, uint16_t) = API->bk_write;
    const uint16_t demod = API->bk_read(BK_DEMOD);
    const uint16_t afc = API->bk_read(BK_AFC);

    if (s.cfg.flags & FLAG_AM) {
        write(BK_DEMOD, demod | 1u);
        write(0x42, 0x6f5c);
        write(0x2a, 0x7434);
        write(BK_AFC, afc | 0x10u);
    } else {
        write(BK_DEMOD, demod & ~1u);
        write(0x42, 0x6b5a);
        write(0x2a, 0x7400);
        write(0x2b, 0x0000);
        write(0x2f, 0x9890);
        write(BK_AFC, afc & ~0x10u);
    }
}

static void selectVhfPath(void)
{
    const uint16_t gpio = API->bk_read(BK_GPIO);
    API->bk_write(BK_GPIO, (uint16_t)((gpio & ~LNA_UHF) | LNA_VHF));
}

/* Next channel in direction dir; the scan skips the skip list (unless it holds them all). */
static void move(int8_t dir, bool skip)
{
    uint8_t ch = s.cfg.channel;
    for (uint8_t i = 0; i < CHANNELS; i++) {
        ch = dir > 0 ? (uint8_t)(ch + 1u == CHANNELS ? 0u : ch + 1u)
                     : (uint8_t)(ch == 0u ? CHANNELS - 1u : ch - 1u);
        if (!skip || !isSkipped(ch))
            break;
    }
    s.cfg.channel = ch;
    tune();
}

/* ---- squelch and scanner ---- */

/* Squelch relative to the noise: the floor follows the averaged RSSI down
 * quickly and up slowly (~1.3 s) while nothing is received, and the speaker
 * opens sq dB above it. The averaged RSSI has to stay past the threshold for
 * several samples: single noise spikes on 27 MHz must not open it. */
static int16_t threshold(void)
{
    return (int16_t)((s.floorQ4 >> 4) + s.cfg.sq);
}

static void track(uint32_t now)
{
    const int32_t raw = API->rssi_dbm();
    if (now - s.tunedAt < SETTLE_MS) {
        s.rssiQ4 = raw * 16;
        s.rssi = (int16_t)raw;
        s.count = 0;
        return;
    }
    s.rssiQ4 += (raw * 16 - s.rssiQ4) >> 2;
    s.rssi = (int16_t)(s.rssiQ4 >> 4);

    if (!s.floorSet) {
        s.floorQ4 = s.rssiQ4;
        s.floorSet = true;
    } else if (s.state != RECEIVING && s.count == 0) {
        const int32_t d = s.rssiQ4 - s.floorQ4;
        s.floorQ4 += d < 0 ? d >> 2 : d >> 7;
    }

    const uint8_t previous = s.state;
    if (s.state == RECEIVING) {
        if (s.rssi >= threshold() - SQ_HYSTERESIS) {
            s.count = 0;
            s.signalAt = now;
        } else if (++s.count >= CLOSE_SAMPLES) {
            s.count = 0;
            s.state = WAITING;
        }
    } else if (s.rssi >= threshold()) {
        if (++s.count >= OPEN_SAMPLES) {
            s.count = 0;
            s.state = RECEIVING;
            s.signalAt = now;
            s.last = s.cfg.channel;
        }
    } else {
        s.count = 0;
        if (s.state == WAITING && now - s.signalAt >= RESUME_MS)
            s.state = IDLE;
    }

    setAudio(s.state == RECEIVING || s.monitor);
    if (s.state != previous)
        s.redraw = true;

    if (s.scan && !s.monitor && s.state == IDLE && s.count == 0 &&
        now - s.tunedAt >= DWELL_MS)
        move(+1, true);
}

/* ---- display ---- */

static char *putText(char *o, const char *t){ while (*t) *o++ = *t++; return o; }

__attribute__((noinline))
static char *putNumber(char *o, int32_t v){ return API->append_i32(o, v); }

static char *putChannel(char *o, uint8_t ch)
{
    if (ch < 9u)
        *o++ = '0';
    return putNumber(o, ch + 1);
}

static int32_t toPixel(int32_t dbm)
{
    int32_t x = (dbm - BAR_MIN) * 11 / 8;            /* -126..-50 dBm -> 0..104 px */
    return x < 0 ? 0 : x > BAR_END ? BAR_END : x;
}

static void draw(void)
{
    static const char *const stateText[] = { "", "RX", "WAIT" };
    char b[28], *o;
    uint8_t (*const screen)[128] = API->fb;

    API->display_clear();
    API->status_clear();
    API->draw_battery();
    /* key hints: "(*)" starts the scan, "(3)" switches the raster; the scan
     * label is highlighted while scanning */
    if (s.scan)
        API->print_inverse("SCAN (*)", 2, 0, true, true, 34);
    else
        API->print_tiny("SCAN (*)", 2, 1, true, true);
    o = putText(b, s.monitor ? "MON " : "");
    *putText(o, stateText[s.state]) = '\0';
    API->print_tiny(b, 38, 1, true, true);

    API->print_bold("CB SKANER", 0, 0, 0);
    o = putText(b, (s.cfg.flags & FLAG_AM) ? "AM " : "FM ");
    *putText(o, (s.cfg.flags & FLAG_ZERO) ? "0 (3)" : "5 (3)") = '\0';
    API->print_normal(b, 72, 0, 0);

    o = putChannel(putText(b, "CH "), s.cfg.channel);
    *o = '\0';
    API->print_string(b, 0, 127, 1, 10);
    if (isSkipped(s.cfg.channel))
        API->print_tiny("SKIP", 104, 12, false, true);

    API->draw_frequency(channelKhz(s.cfg.channel) * 100u, 16, 3);

    /* signal bar on line 5, the vertical mark is the squelch threshold */
    const int32_t len = toPixel(s.rssi);
    for (int32_t x = 0; x < len; x++)
        screen[5][x] = 0x18;
    screen[5][toPixel(threshold())] = 0x7E;
    API->print_tiny("(1-7)", 108, 41, false, true);     /* squelch keys */

    o = putNumber(b, s.rssi);
    o = putText(o, " N");
    o = putNumber(o, s.floorQ4 >> 4);
    o = putText(o, " SQ+");
    o = putNumber(o, s.cfg.sq);
    if (s.last != NONE)
        o = putChannel(putText(o, " L"), s.last);
    *o = '\0';
    /* the 3x5 font is 6 rows tall: from y = 49 it stays inside line 6 */
    API->print_tiny(b, 0, 49, false, true);
    /* version and callsign, right-aligned: 4 px per character */
    API->print_tiny(SIGNATURE, (uint8_t)(128u - 4u * (sizeof SIGNATURE - 1u)), 49, false, true);

    API->blit_status();
    API->blit_full();
    s.redraw = false;
}

/* ---- keys ---- */

static void onKey(uint8_t key)
{
    switch (key) {
    case APP_KEY_UP:
    case APP_KEY_DOWN:
        s.scan = false;
        move(API->nav_dir(key), false);
        break;
    case APP_KEY_STAR:
        s.scan = !s.scan;
        break;
    case APP_KEY_MENU:
        s.cfg.flags ^= FLAG_AM;
        setModulation();
        break;
    case APP_KEY_1:
        if (s.cfg.sq > SQ_MIN) s.cfg.sq = (int8_t)(s.cfg.sq - SQ_STEP);
        break;
    case APP_KEY_7:
        if (s.cfg.sq < SQ_MAX) s.cfg.sq = (int8_t)(s.cfg.sq + SQ_STEP);
        break;
    case APP_KEY_3:
        s.cfg.flags ^= FLAG_ZERO;
        tune();
        break;
    case APP_KEY_5:
        s.cfg.skipped[s.cfg.channel >> 3] ^= (uint8_t)(1u << (s.cfg.channel & 7u));
        break;
    case APP_KEY_8:
        if (s.last != NONE) {
            s.scan = false;
            s.cfg.channel = s.last;
            tune();
        }
        break;
    case APP_KEY_9:
        s.scan = false;
        s.cfg.channel = CHANNEL_19;
        tune();
        break;
    case APP_KEY_0:
        s.monitor = !s.monitor;
        break;
    case APP_KEY_EXIT:
        s.running = false;
        break;
    default:
        return;                 /* PTT and the rest: nothing, the app never transmits */
    }
    s.redraw = true;
}

static void pollKeys(uint32_t now)
{
    const uint8_t k = API->get_key();

    if (k == APP_KEY_SAVER)
        return;
    if (k == APP_KEY_WAKE) {
        s.redraw = true;
        return;
    }
    if (k == APP_KEY_INVALID) {
        s.key = APP_KEY_INVALID;
        return;
    }
    if (k != s.key) {
        s.key = k;
        s.pressedAt = now;
        API->backlight_on();
        onKey(k);
    } else if ((k == APP_KEY_UP || k == APP_KEY_DOWN) && now - s.pressedAt >= 400u) {
        s.pressedAt = now - 300u;   /* held: every 100 ms */
        onKey(k);
    }
}

/* ---- start and exit ---- */

static void loadSettings(void)
{
    settings_t c;
    API->cfg_load((uint8_t *)&c, sizeof c);
    if (c.magic == CFG_MAGIC && c.channel < CHANNELS && c.flags <= 3u &&
        c.sq >= SQ_MIN && c.sq <= SQ_MAX) {
        s.cfg = c;
        return;
    }
    s.cfg.magic = CFG_MAGIC;  /* defaults: channel 19, AM, raster "0", 10 dB above noise */
    s.cfg.channel = CHANNEL_19;
    s.cfg.flags = FLAG_AM | FLAG_ZERO;
    s.cfg.sq = SQ_DEFAULT;
}

static void waitKeyRelease(void)
{
    for (uint8_t quiet = 0; quiet < 2u; ) {
        quiet = API->get_key() == APP_KEY_INVALID ? (uint8_t)(quiet + 1u) : 0u;
        API->delay_ms(10);
        API->backlight_update();
    }
}

__attribute__((section(".text.entry"), used))
void app_main(const app_api_t *api)
{
    s.api = api;
    API->backlight_on();
    loadSettings();

    s.last = NONE;
    s.key = APP_KEY_MENU;       /* the launch key may still be held */
    s.ctrl = API->bk_read(BK_CTRL);
    s.audio = true;             /* unknown at launch: the first setAudio(false) mutes */
    selectVhfPath();
    setModulation();
    tune();

    uint32_t batteryAt = 0;
    for (s.running = true; s.running; ) {
        const uint32_t now = API->ticks_ms();

        pollKeys(now);
        track(now);

        if (now - batteryAt >= 1000u) {
            batteryAt = now;
            API->battery_sample();
        }
        if (s.scan || s.state == RECEIVING)
            API->backlight_on();
        if (s.redraw || now - s.drawnAt >= 200u) {
            s.drawnAt = now;
            draw();
        }
        API->delay_ms(10);
        API->backlight_update();
    }

    API->audio_path(false);
    waitKeyRelease();
    API->cfg_save((const uint8_t *)&s.cfg, sizeof s.cfg);
}
