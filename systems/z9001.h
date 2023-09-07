#pragma once
/*#
    # z9001.h

    A Robotron Z9001/KC87 emulator in a C header.

    Do this:
    ~~~C
    #define CHIPS_IMPL
    ~~~
    before you include this file in *one* C or C++ file to create the
    implementation.

    Optionally provide the following macros with your own implementation

    ~~~C
    CHIPS_ASSERT(c)
    ~~~
        your own assert macro (default: assert(c))

    You need to include the following headers before including z9001.h:

    - chips/chips_common.h
    - chips/z80.h
    - chips/z80pio.h
    - chips/z80ctc.h
    - chips/beeper.h
    - chips/mem.h
    - chips/kbd.h
    - chips/clk.h

    ## The Robotron Z9001

    The Z9001 (later retconned to KC85/1) was independently developed
    to the HC900 (aka KC85/2) by Robotron Dresden. It had a pretty
    slick design with an integrated keyboard which was legendary for
    how hard it was to type on.\n\nThe standard model had 16 KByte RAM,
    a monochrome 40x24 character display, and a 2.5 MHz U880 CPU
    (making it the fastest East German 8-bitter). The Z9001 could
    be extended by up to 4 expansion modules, usually one or two
    16 KByte RAM modules, and a 10 KByte ROM BASIC module (the
    version emulated here comes with 32 KByte RAM and a BASIC module).

    ## The Robotron KC87

    The KC87 was the successor to the KC85/1. The only real difference
    was the built-in BASIC interpreter in ROM. The KC87 emulated here
    has 48 KByte RAM and the video color extension which could
    assign 8 foreground and 8 (identical) background colors per character,
    plus a blinking flag. This video extension was already available on the
    Z9001 though.

    ## TODO:
    - enable/disable audio on PIO1-A bit 7
    - border color
    - 40x20 video mode

    ## Reference Info

    - schematics: http://www.sax.de/~zander/kc/kcsch_1.pdf
    - manual: http://www.sax.de/~zander/z9001/doku/z9_fub.pdf

    ## zlib/libpng license

    Copyright (c) 2018 Andre Weissflog
    This software is provided 'as-is', without any express or implied warranty.
    In no event will the authors be held liable for any damages arising from the
    use of this software.
    Permission is granted to anyone to use this software for any purpose,
    including commercial applications, and to alter it and redistribute it
    freely, subject to the following restrictions:
        1. The origin of this software must not be misrepresented; you must not
        claim that you wrote the original software. If you use this software in a
        product, an acknowledgment in the product documentation would be
        appreciated but is not required.
        2. Altered source versions must be plainly marked as such, and must not
        be misrepresented as being the original software.
        3. This notice may not be removed or altered from any source
        distribution.
#*/
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdalign.h>

#ifdef __cplusplus
extern "C" {
#endif

// bump this whenever the z9001_t struct layout changes
#define Z9001_SNAPSHOT_VERSION (0x0001)

#define Z9001_MAX_AUDIO_SAMPLES (1024)      // max number of audio samples in internal sample buffer
#define Z9001_DEFAULT_AUDIO_SAMPLES (128)   // default number of samples in internal sample buffer
#define Z9001_FRAMEBUFFER_WIDTH (512)
#define Z9001_FRAMEBUFFER_HEIGHT (192)
#define Z9001_FRAMEBUFFER_SIZE_BYTES (Z9001_FRAMEBUFFER_WIDTH * Z9001_FRAMEBUFFER_HEIGHT)
#define Z9001_DISPLAY_WIDTH (320)
#define Z9001_DISPLAY_HEIGHT (192)

// Z9001/KC87 model types
typedef enum {
    Z9001_TYPE_Z9001,   // the original Z9001 (default)
    Z9001_TYPE_KC87,    // the revised KC87 with built-in BASIC and color module
} z9001_type_t;

// configuration parameters for z9001_init()
typedef struct {
    z9001_type_t type;                  // default is Z9001_TYPE_Z9001
    chips_debug_t debug;                // optional debug hook
    chips_audio_desc_t audio;
    struct {
        // Z9001 ROM images
        struct {
            chips_range_t os_1;
            chips_range_t os_2;
            chips_range_t font;
            // optional BASIC module ROM
            chips_range_t basic;
        } z9001;
        // KC85 ROM images
        struct {
            chips_range_t os;
            chips_range_t basic;
            chips_range_t font;
        } kc87;
    } roms;
} z9001_desc_t;

// Z9001 emulator state
typedef struct {
    z80_t cpu;
    z80pio_t pio1;
    z80pio_t pio2;
    z80ctc_t ctc;
    beeper_t beeper;
    uint8_t blink_flip_flop;    // bit 7 0=>1=>0
    z9001_type_t type;
    uint64_t pins;
    uint64_t ctc_zcto2;         // pin mask to store state of CTC ZCTO2
    uint32_t blink_counter;
    // FIXME: uint8_t border_color;
    mem_t mem;
    kbd_t kbd;

    bool valid;
    bool z9001_has_basic_rom;
    chips_debug_t debug;

    struct {
        chips_audio_callback_t callback;
        int num_samples;
        int sample_pos;
        float sample_buffer[Z9001_MAX_AUDIO_SAMPLES];
    } audio;
    uint8_t ram[1<<16];
    uint8_t rom[0x4000];
    uint8_t rom_font[0x0800];   // 2 KB font ROM (not mapped into CPU address space)
    alignas(64) uint8_t fb[Z9001_FRAMEBUFFER_SIZE_BYTES];
} z9001_t;

// initialize a new Z9001 instance
void z9001_init(z9001_t* sys, const z9001_desc_t* desc);
// discard a Z9001 instance
void z9001_discard(z9001_t* sys);
// reset Z9001 instance
void z9001_reset(z9001_t* sys);
// query information about display requirements, can be called with nullptr
chips_display_info_t z9001_display_info(z9001_t* sys);
// run Z9001 instance for a given number of microseconds, return number of executed ticks
uint32_t z9001_exec(z9001_t* sys, uint32_t micro_seconds);
// send a key-down event
void z9001_key_down(z9001_t* sys, int key_code);
// send a key-up event
void z9001_key_up(z9001_t* sys, int key_code);
// load a KC TAP or KCC file into the emulator
bool z9001_quickload(z9001_t* sys, chips_range_t data);
// save a snapshot, patches any pointers to zero, returns a snapshot version
uint32_t z9001_save_snapshot(z9001_t* sys, z9001_t* dst);
// load a snapshot, returns false if snapshot version doesn't match
bool z9001_load_snapshot(z9001_t* sys, uint32_t version, const z9001_t* src);

#ifdef __cplusplus
} /* extern "C" */
#endif

/*-- IMPLEMENTATION ----------------------------------------------------------*/
#ifdef CHIPS_IMPL
#include <string.h>
#ifndef CHIPS_ASSERT
    #include <assert.h>
    #define CHIPS_ASSERT(c) assert(c)
#endif

// xorshift randomness for memory initialization
static inline uint32_t _z9001_xorshift32(uint32_t x) {
    x ^= x<<13;
    x ^= x>>17;
    x ^= x<<5;
    return x;
}

#define _Z9001_DEFAULT(val,def) (((val) != 0) ? (val) : (def))
#define _Z9001_FREQUENCY (2457600)

// IO address decoding masks and pins
#define _Z9001_IO_SEL_MASK (Z80_IORQ|Z80_M1|Z80_A7|Z80_A6)
#define _Z9001_IO_SEL_PINS (Z80_IORQ|Z80_A7)
// CTC is mapped to ports 0x80 to 0x87 (each port is mapped twice)
#define _Z9001_CTC_SEL_MASK (_Z9001_IO_SEL_MASK|Z80_A5|Z80_A4|Z80_A3)
#define _Z9001_CTC_SEL_PINS (_Z9001_IO_SEL_PINS)
// PIO1 is mapped to ports 0x88 to 0x8F (each port is mapped twice)
#define _Z9001_PIO1_SEL_MASK (_Z9001_IO_SEL_MASK|Z80_A5|Z80_A4|Z80_A3)
#define _Z9001_PIO1_SEL_PINS (_Z9001_IO_SEL_PINS|Z80_A3)
// PIO2 is mapped to ports 0x90 to 0x97 (each port is mapped twice)
#define _Z9001_PIO2_SEL_MASK (_Z9001_IO_SEL_MASK|Z80_A5|Z80_A4|Z80_A3)
#define _Z9001_PIO2_SEL_PINS (_Z9001_IO_SEL_PINS|Z80_A4)

void z9001_init(z9001_t* sys, const z9001_desc_t* desc) {
    CHIPS_ASSERT(sys && desc);
    if (desc->debug.callback.func) { CHIPS_ASSERT(desc->debug.stopped); }

    memset(sys, 0, sizeof(z9001_t));
    sys->valid = true;
    sys->type = desc->type;
    sys->debug = desc->debug;
    if (desc->type == Z9001_TYPE_Z9001) {
        CHIPS_ASSERT(desc->roms.z9001.font.ptr && (desc->roms.z9001.font.size == sizeof(sys->rom_font)));
        memcpy(sys->rom_font, desc->roms.z9001.font.ptr, sizeof(sys->rom_font));
        if (desc->roms.z9001.basic.ptr) {
            CHIPS_ASSERT(desc->roms.z9001.basic.size == 0x2800);
            memcpy(&sys->rom[0x0000], desc->roms.z9001.basic.ptr, 0x2800);
            sys->z9001_has_basic_rom = true;
        }
        CHIPS_ASSERT(desc->roms.z9001.os_1.ptr && (desc->roms.z9001.os_1.size == 0x0800));
        memcpy(&sys->rom[0x3000], desc->roms.z9001.os_1.ptr, 0x0800);
        CHIPS_ASSERT(desc->roms.z9001.os_2.ptr && (desc->roms.z9001.os_2.size == 0x0800));
        memcpy(&sys->rom[0x3800], desc->roms.z9001.os_2.ptr, 0x0800);
    }
    else {
        CHIPS_ASSERT(desc->roms.kc87.font.ptr && (desc->roms.kc87.font.size == sizeof(sys->rom_font)));
        memcpy(sys->rom_font, desc->roms.kc87.font.ptr, sizeof(sys->rom_font));
        CHIPS_ASSERT(desc->roms.kc87.basic.ptr && (desc->roms.kc87.basic.size == 0x2000));
        memcpy(&sys->rom[0x0000], desc->roms.kc87.basic.ptr, 0x2000);
        CHIPS_ASSERT(desc->roms.kc87.os.ptr && (desc->roms.kc87.os.size == 0x2000));
        memcpy(&sys->rom[0x2000], desc->roms.kc87.os.ptr, 0x2000);
    }

    // initialize the hardware
    z80_init(&sys->cpu);
    z80ctc_init(&sys->ctc);
    z80pio_init(&sys->pio1);
    z80pio_init(&sys->pio2);

    sys->audio.callback = desc->audio.callback;
    sys->audio.num_samples = _Z9001_DEFAULT(desc->audio.num_samples, Z9001_DEFAULT_AUDIO_SAMPLES);
    CHIPS_ASSERT(sys->audio.num_samples <= Z9001_MAX_AUDIO_SAMPLES);
    beeper_init(&sys->beeper, &(beeper_desc_t){
        .tick_hz = _Z9001_FREQUENCY,
        .sound_hz = _Z9001_DEFAULT(desc->audio.sample_rate, 44100),
        .base_volume = _Z9001_DEFAULT(desc->audio.volume, 0.5f),
    });

    /* setup memory map:
        - memory mapping is static and cannot be changed
        - initial memory state is random
        - configure the Z9001 with an additional 16 KB RAM module, and BASIC module
        - configure the KC87 with 48 KByte RAM and color module
        - 1 KB ASCII frame buffer at 0xEC00
        - KC87 has a 1 KB color buffer at 0xEC800
    */
    uint32_t r = 0x6D98302B;
    for (size_t i = 0; i < sizeof(sys->ram);) {
        r = _z9001_xorshift32(r);
        sys->ram[i++] = r;
        sys->ram[i++] = (r>>8);
        sys->ram[i++] = (r>>16);
        sys->ram[i++] = (r>>24);
    }
    mem_init(&sys->mem);
    if (Z9001_TYPE_Z9001 == sys->type) {
        // 16 KB RAM + 16 KB RAM module
        mem_map_ram(&sys->mem, 0, 0x0000, 0x8000, sys->ram);
        // optional 12 KB BASIC ROM module at 0xC000
        if (desc->roms.z9001.basic.ptr) {
            mem_map_rom(&sys->mem, 1, 0xC000, 0x2800, &sys->rom[0x0000]);
        }
        // 2 OS ROMs at 2 KB each
        mem_map_rom(&sys->mem, 1, 0xF000, 0x0800, &sys->rom[0x3000]);
        mem_map_rom(&sys->mem, 1, 0xF800, 0x0800, &sys->rom[0x3800]);
    }
    else {
        // 48 KB RAM
        mem_map_ram(&sys->mem, 0, 0x0000, 0xC000, sys->ram);
        // 1 KB color RAM
        mem_map_ram(&sys->mem, 0, 0xE800, 0x0400, &sys->ram[0xE800]);
        // 8 KB builtin BASIC
        mem_map_rom(&sys->mem, 1, 0xC000, 0x2000, &sys->rom[0x0000]);
        // 8 KB ROM (overlayed by 1 KB at 0xEC00 for ASCII video RAM)
        mem_map_rom(&sys->mem, 1, 0xE000, 0x2000, &sys->rom[0x2000]);
    }
    // 1 KB ASCII video RAM
    mem_map_ram(&sys->mem, 0, 0xEC00, 0x0400, &sys->ram[0xEC00]);

    /* setup the 8x8 keyboard matrix, keep pressed keys sticky for 3 frames
        to give the keyboard scanning code enough time to read the key
    */
    kbd_init(&sys->kbd, 3);
    // shift key is column 0, line 7
    kbd_register_modifier(&sys->kbd, 0, 0, 7);
    // register alpha-numeric keys
    const char* keymap =
        // unshifted keys
        "01234567"
        "89:;,=.?"
        "@ABCDEFG"
        "HIJKLMNO"
        "PQRSTUVW"
        "XYZ   ^ "
        "        "
        "        "
        // shifted keys
        "_!\"#$%&'"
        "()*+<->/"
        " abcdefg"
        "hijklmno"
        "pqrstuvw"
        "xyz     "
        "        "
        "        ";
    for (int shift = 0; shift < 2; shift++) {
        for (int line = 0; line < 8; line++) {
            for (int column = 0; column < 8; column++) {
                int c = keymap[shift*64 + line*8 + column];
                if (c != 0x20) {
                    kbd_register_key(&sys->kbd, c, column, line, shift?(1<<0):0);
                }
            }
        }
    }
    // special keys
    kbd_register_key(&sys->kbd, 0x03, 6, 6, 0);      // stop (Esc)
    kbd_register_key(&sys->kbd, 0x08, 0, 6, 0);      // cursor left
    kbd_register_key(&sys->kbd, 0x09, 1, 6, 0);      // cursor right
    kbd_register_key(&sys->kbd, 0x0A, 2, 6, 0);      // cursor up
    kbd_register_key(&sys->kbd, 0x0B, 3, 6, 0);      // cursor down
    kbd_register_key(&sys->kbd, 0x0D, 5, 6, 0);      // enter
    kbd_register_key(&sys->kbd, 0x13, 4, 5, 0);      // pause
    kbd_register_key(&sys->kbd, 0x14, 1, 7, 0);      // color
    kbd_register_key(&sys->kbd, 0x19, 3, 5, 0);      // home
    kbd_register_key(&sys->kbd, 0x1A, 5, 5, 0);      // insert
    kbd_register_key(&sys->kbd, 0x1B, 4, 6, 0);      // esc (Shift+Esc)
    kbd_register_key(&sys->kbd, 0x1C, 4, 7, 0);      // list
    kbd_register_key(&sys->kbd, 0x1D, 5, 7, 0);      // run
    kbd_register_key(&sys->kbd, 0x20, 7, 6, 0);      // space

    // execution starts at 0xF000
    sys->pins = z80_prefetch(&sys->cpu, 0xF000);
}

void z9001_discard(z9001_t* sys) {
    CHIPS_ASSERT(sys && sys->valid);
    sys->valid = false;
}

void z9001_reset(z9001_t* sys) {
    CHIPS_ASSERT(sys && sys->valid);
    z80_reset(&sys->cpu);
    z80pio_reset(&sys->pio1);
    z80pio_reset(&sys->pio2);
    z80ctc_reset(&sys->ctc);
    beeper_reset(&sys->beeper);
    sys->pins = z80_prefetch(&sys->cpu, 0xF000);
}

static uint64_t _z9001_tick(z9001_t* sys, uint64_t pins) {
    pins = z80_tick(&sys->cpu, pins);

    // handle memory requests
    if (pins & Z80_MREQ) {
        const uint16_t addr = Z80_GET_ADDR(pins);
        if (pins & Z80_RD) {
            Z80_SET_DATA(pins, mem_rd(&sys->mem, addr));
        }
        else if (pins & Z80_WR) {
            mem_wr(&sys->mem, addr, Z80_GET_DATA(pins));
        }
    }

    // tick PIO-1 (highest priority daisychain device)
    {
        pins |= Z80_IEIO;
        // PIO1 is mapped to ports 0x88 to 0x8F (each port is mapped twice)
        if ((pins & _Z9001_PIO1_SEL_MASK) == _Z9001_PIO1_SEL_PINS) {
            pins |= Z80PIO_CE;
        }
        if (pins & Z80_A0) { pins |= Z80PIO_BASEL; }
        if (pins & Z80_A1) { pins |= Z80PIO_CDSEL; }
        // no port A/B inputs
        pins = z80pio_tick(&sys->pio1, pins);
        /*
            FIXME:
            PIO1-A bits:
            0..1:    unused
            2:       display mode (0: 24 lines, 1: 20 lines)
            3..5:    border color
            6:       graphics LED on keyboard (0: off)
            7:       enable audio output (1: enabled)

            PIO1-B is reserved for external user devices
        */
        pins &= Z80_PIN_MASK;
    }

    // tick PIO-2
    {
        // PIO2 is mapped to ports 0x90 to 0x97 (each port is mapped twice)
        if ((pins & _Z9001_PIO2_SEL_MASK) == _Z9001_PIO2_SEL_PINS) {
            pins |= Z80PIO_CE;
        }
        if (pins & Z80_A0) { pins |= Z80PIO_BASEL; }
        if (pins & Z80_A1) { pins |= Z80PIO_CDSEL; }

        // NOTE port B input may trigger an interrupt
        const uint8_t pa_in = ~kbd_scan_columns(&sys->kbd);
        const uint8_t pb_in = ~kbd_scan_lines(&sys->kbd);
        Z80PIO_SET_PAB(pins, pa_in, pb_in);
        pins = z80pio_tick(&sys->pio2, pins);
        const uint8_t pa_out = ~Z80PIO_GET_PA(pins);
        const uint8_t pb_out = ~Z80PIO_GET_PB(pins);
        kbd_set_active_columns(&sys->kbd, pa_out);
        kbd_set_active_lines(&sys->kbd, pb_out);

        pins &= Z80_PIN_MASK;
    }

    // tick the CTC
    {
        /* CTC channel 2 output signal ZCTO2 is connected to CTC channel 3 input
           signal CLKTRG3 to form a timer cascade which drives the system clock,
           this is why we need to preserve the CTC ZCTO2 state between ticks
        */
        pins |= sys->ctc_zcto2;
        // CTC is mapped to ports 0x80 to 0x87 (each port is mapped twice)
        if ((pins & _Z9001_CTC_SEL_MASK) == _Z9001_CTC_SEL_PINS) {
            pins |= Z80CTC_CE;
        }
        if (pins & Z80_A0) { pins |= Z80CTC_CS0; };
        if (pins & Z80_A1) { pins |= Z80CTC_CS1; };
        if (pins & Z80CTC_ZCTO2) { pins |= Z80CTC_CLKTRG3; }
        pins = z80ctc_tick(&sys->ctc, pins);
        if (pins & Z80CTC_ZCTO0) {
            // CTC channel 0 controls the beeper frequency
            beeper_toggle(&sys->beeper);
        }
        sys->ctc_zcto2 = pins & Z80CTC_ZCTO2;
        pins &= Z80_PIN_MASK;
    }

    // tick the beeper
    if (beeper_tick(&sys->beeper)) {
        // new audio sample ready
        sys->audio.sample_buffer[sys->audio.sample_pos++] = sys->beeper.sample;
        if (sys->audio.sample_pos == sys->audio.num_samples) {
            if (sys->audio.callback.func) {
                sys->audio.callback.func(sys->audio.sample_buffer, sys->audio.num_samples, sys->audio.callback.user_data);
            }
            sys->audio.sample_pos = 0;
        }
    }

    /* the blink flip flop is controlled by a 'bisync' video signal
        (I guess that means it triggers at half PAL frequency: 25Hz),
        going into a binary counter, bit 4 of the counter is connected
        to the blink flip flop.
    */
    if (0 >= sys->blink_counter--) {
        sys->blink_counter = (_Z9001_FREQUENCY * 8) / 25;
        sys->blink_flip_flop ^= 0x80;
    }
    return pins;
}

static inline void _z9001_decode_8pixels(uint8_t* dst, uint8_t pixels, uint8_t colors) {
    // courtesy of ryg: https://mastodon.gamedev.place/@rygorous/109531596140414988
    static const uint32_t lut32[16] = {
        0x00000000, 0xff000000, 0x00ff0000, 0xffff0000,
        0x0000ff00, 0xff00ff00, 0x00ffff00, 0xffffff00,
        0x000000ff, 0xff0000ff, 0x00ff00ff, 0xffff00ff,
        0x0000ffff, 0xff00ffff, 0x00ffffff, 0xffffffff,
    };
    const uint32_t colors32 = colors * 0x01010101u;
    const uint32_t bg32 = colors32 & 0x07070707;
    const uint32_t fg32 = (colors32 >> 4) & 0x07070707;
    const uint32_t xor32 = bg32 ^ fg32;
    uint32_t* dst32 = (uint32_t*)dst;
    dst32[0] = bg32 ^ (xor32 & lut32[pixels >> 4]);
    dst32[1] = bg32 ^ (xor32 & lut32[pixels & 0xf]);
}

static void _z9001_decode_vidmem(z9001_t* sys) {
    // FIXME: there's also a 40x20 video mode
    const uint8_t* vidmem = &sys->ram[0xEC00];     // 1 KB ASCII buffer at EC00
    const uint8_t* colmem = &sys->ram[0xE800];     // 1 KB color buffer at E800
    size_t offset = 0;
    if (Z9001_TYPE_KC87 == sys->type) {
        // KC87 with color module
        const uint8_t* font = sys->rom_font;
        for (size_t y = 0; y < 24; y++) {
            for (size_t py = 0; py < 8; py++) {
                uint8_t* dst = &sys->fb[(y * 8 + py) * Z9001_FRAMEBUFFER_WIDTH];
                for (size_t x = 0; x < 40; x++, dst += 8) {
                    uint8_t chr = vidmem[offset+x];
                    uint8_t pixels = font[(chr<<3)|py];
                    uint8_t colors = colmem[offset+x];
                    if (colors & sys->blink_flip_flop & 0x80) {
                        // blinking: swap back- and foreground color
                        colors = ((colors & 7) << 4) | ((colors >> 4) & 7);
                    }
                    _z9001_decode_8pixels(dst, pixels, colors);
                }
            }
            offset += 40;
        }
    }
    else {
        // Z9001 monochrome display
        const uint8_t* font = sys->rom_font;
        for (size_t y = 0; y < 24; y++) {
            for (size_t py = 0; py < 8; py++) {
                uint8_t* dst = &sys->fb[(y * 8 + py) * Z9001_FRAMEBUFFER_WIDTH];
                for (size_t x = 0; x < 40; x++, dst += 8) {
                    uint8_t chr = vidmem[offset + x];
                    uint8_t pixels = font[(chr<<3)|py];
                    _z9001_decode_8pixels(dst, pixels, 0x70);
                }
            }
            offset += 40;
        }
    }
}

uint32_t z9001_exec(z9001_t* sys, uint32_t micro_seconds) {
    CHIPS_ASSERT(sys && sys->valid);
    const uint32_t num_ticks = clk_us_to_ticks(_Z9001_FREQUENCY, micro_seconds);
    uint64_t pins = sys->pins;
    if (0 == sys->debug.callback.func) {
        // run without debug hook
        for (uint32_t tick = 0; tick < num_ticks; tick++) {
            pins = _z9001_tick(sys, pins);
        }
    }
    else {
        // run with debug hook
        for (uint32_t tick = 0; (tick < num_ticks) && !(*sys->debug.stopped); tick++) {
            pins = _z9001_tick(sys, pins);
            sys->debug.callback.func(sys->debug.callback.user_data, pins);
        }
    }
    sys->pins = pins;
    kbd_update(&sys->kbd, micro_seconds);
    _z9001_decode_vidmem(sys);
    return num_ticks;
}

void z9001_key_down(z9001_t* sys, int key_code) {
    CHIPS_ASSERT(sys && sys->valid);
    kbd_key_down(&sys->kbd, key_code);
    /* FIXME FIXME FIXME keyboard matrix lines are directly connected to the PIO2's Port B */
    //z80pio_write_port(&sys->pio2, Z80PIO_PORT_B, ~kbd_scan_lines(&sys->kbd));
}

void z9001_key_up(z9001_t* sys, int key_code) {
    CHIPS_ASSERT(sys && sys->valid);
    kbd_key_up(&sys->kbd, key_code);
    /* FIXME FIXME FIXME keyboard matrix lines are directly connected to the PIO2's Port B */
    //z80pio_write_port(&sys->pio2, Z80PIO_PORT_B, ~kbd_scan_lines(&sys->kbd));
}

// common start function for file loading routines
static void _z9001_load_start(z9001_t* sys, uint16_t exec_addr) {
    sys->cpu.a = 0x00; sys->cpu.f = 0x10;
    sys->cpu.bc = sys->cpu.bc2 = 0x0000;
    sys->cpu.de = sys->cpu.de2 = 0x0000;
    sys->cpu.hl = sys->cpu.hl2 = 0x0000;
    sys->cpu.af2 = 0x0000;
    sys->pins = z80_prefetch(&sys->cpu, exec_addr);
}

// KCC file format support
typedef struct {
    uint8_t name[16];
    uint8_t num_addr;
    uint8_t load_addr_l;
    uint8_t load_addr_h;
    uint8_t end_addr_l;
    uint8_t end_addr_h;
    uint8_t exec_addr_l;
    uint8_t exec_addr_h;
    uint8_t pad[128 - 23];  // pad to 128 bytes
} _z9001_kcc_header;

// KCC files cannot really be identified since they have no magic number
static bool _z9001_is_valid_kcc(chips_range_t data) {
    if (data.size <= sizeof(_z9001_kcc_header)) {
        return false;
    }
    const _z9001_kcc_header* hdr = (const _z9001_kcc_header*)data.ptr;
    for (int i = 0; i < 16; i++) {
        if (hdr->name[i] >= 128) {
            return false;
        }
    }
    if (hdr->num_addr > 3) {
        return false;
    }
    uint16_t load_addr = hdr->load_addr_h<<8 | hdr->load_addr_l;
    uint16_t end_addr = hdr->end_addr_h<<8 | hdr->end_addr_l;
    if (end_addr <= load_addr) {
        return false;
    }
    if (hdr->num_addr > 2) {
        uint16_t exec_addr = hdr->exec_addr_h<<8 | hdr->exec_addr_l;
        if ((exec_addr < load_addr) || (exec_addr > end_addr)) {
            return false;
        }
    }
    size_t required_len = (end_addr - load_addr) + sizeof(_z9001_kcc_header);
    if (required_len > data.size) {
        return false;
    }
    // could be a KCC file
    return true;
}

static bool _z9001_load_kcc(z9001_t* sys, chips_range_t data) {
    const uint8_t* ptr = data.ptr;
    const _z9001_kcc_header* hdr = (_z9001_kcc_header*)ptr;
    uint16_t addr = hdr->load_addr_h<<8 | hdr->load_addr_l;
    uint16_t end_addr  = hdr->end_addr_h<<8 | hdr->end_addr_l;
    ptr += sizeof(_z9001_kcc_header);
    while (addr < end_addr) {
        // data is continuous
        mem_wr(&sys->mem, addr++, *ptr++);
    }
    return false;
}

// KC TAP file format support
typedef struct {
    uint8_t sig[16];        // "\xC3KC-TAPE by AF. "
    uint8_t type;           // 00: KCTAP_Z9001, 01: KCTAP_KC85, else: KCTAP_SYS
    _z9001_kcc_header kcc;  // from here on identical with KCC
} _z9001_kctap_header;

static bool _z9001_is_valid_kctap(chips_range_t data) {
    if (data.size <= sizeof(_z9001_kctap_header)) {
        return false;
    }
    const _z9001_kctap_header* hdr = (const _z9001_kctap_header*)data.ptr;
    static const uint8_t sig[16] = { 0xC3,'K','C','-','T','A','P','E',0x20,'b','y',0x20,'A','F','.',0x20 };
    for (size_t i = 0; i < 16; i++) {
        if (sig[i] != hdr->sig[i]) {
            return false;
        }
    }
    if (hdr->kcc.num_addr > 3) {
        return false;
    }
    uint16_t load_addr = hdr->kcc.load_addr_h<<8 | hdr->kcc.load_addr_l;
    uint16_t end_addr = hdr->kcc.end_addr_h<<8 | hdr->kcc.end_addr_l;
    if (end_addr <= load_addr) {
        return false;
    }
    if (hdr->kcc.num_addr > 2) {
        uint16_t exec_addr = hdr->kcc.exec_addr_h<<8 | hdr->kcc.exec_addr_l;
        if ((exec_addr < load_addr) || (exec_addr > end_addr)) {
            return false;
        }
    }
    size_t required_len = (end_addr - load_addr) + sizeof(_z9001_kctap_header);
    if (required_len > data.size) {
        return false;
    }
    // this appears to be a valid KC TAP file
    return true;
}

static bool _z9001_load_kctap(z9001_t* sys, chips_range_t data) {
    const uint8_t* ptr = data.ptr;
    const _z9001_kctap_header* hdr = (const _z9001_kctap_header*)ptr;
    uint16_t addr = hdr->kcc.load_addr_h<<8 | hdr->kcc.load_addr_l;
    uint16_t end_addr  = hdr->kcc.end_addr_h<<8 | hdr->kcc.end_addr_l;
    ptr += sizeof(_z9001_kctap_header);
    while (addr < end_addr) {
        // each block is 1 lead-byte + 128 bytes data
        ptr++;
        for (int i = 0; i < 128; i++) {
            mem_wr(&sys->mem, addr++, *ptr++);
        }
    }
    // if file has an exec-address, start the program
    if (hdr->kcc.num_addr > 2) {
        _z9001_load_start(sys, hdr->kcc.exec_addr_h<<8 | hdr->kcc.exec_addr_l);
    }
    return true;
}

bool z9001_quickload(z9001_t* sys, chips_range_t data) {
    CHIPS_ASSERT(sys && sys->valid && data.ptr);
    // first check for KC TAP, since this can be properly identified
    if (_z9001_is_valid_kctap(data)) {
        return _z9001_load_kctap(sys, data);
    }
    else if (_z9001_is_valid_kcc(data)) {
        return _z9001_load_kcc(sys, data);
    }
    else {
        // not a known file type, or not enough data
        return false;
    }
}

chips_display_info_t z9001_display_info(z9001_t* sys) {
    static const uint32_t palette[8] = {
        0xFF000000,     // black
        0xFF0000FF,     // red
        0xFF00FF00,     // green
        0xFF00FFFF,     // yellow
        0xFFFF0000,     // blue
        0xFFFF00FF,     // purple
        0xFFFFFF00,     // cyan
        0xFFFFFFFF,     // white
    };
    const chips_display_info_t res = {
        .frame = {
            .dim = {
                .width = Z9001_FRAMEBUFFER_WIDTH,
                .height = Z9001_FRAMEBUFFER_HEIGHT,
            },
            .buffer = {
                .ptr = sys ? sys->fb : 0,
                .size = Z9001_FRAMEBUFFER_SIZE_BYTES,
            },
            .bytes_per_pixel = 1,
        },
        .screen = {
            .x = 0,
            .y = 0,
            .width = Z9001_DISPLAY_WIDTH,
            .height = Z9001_DISPLAY_HEIGHT,
        },
        .palette = {
            .ptr = (void*)palette,
            .size = sizeof(palette)
        }
    };
    CHIPS_ASSERT(((sys == 0) && (res.frame.buffer.ptr == 0)) || ((sys != 0) && (res.frame.buffer.ptr != 0)));
    return res;
}

uint32_t z9001_save_snapshot(z9001_t* sys, z9001_t* dst) {
    CHIPS_ASSERT(sys && dst);
    *dst = *sys;
    chips_debug_snapshot_onsave(&dst->debug);
    chips_audio_callback_snapshot_onsave(&dst->audio.callback);
    mem_snapshot_onsave(&dst->mem, sys);
    return Z9001_SNAPSHOT_VERSION;
}

bool z9001_load_snapshot(z9001_t* sys, uint32_t version, const z9001_t* src) {
    CHIPS_ASSERT(sys && src);
    if (version != Z9001_SNAPSHOT_VERSION) {
        return false;
    }
    static z9001_t im;
    im = *src;
    chips_debug_snapshot_onload(&im.debug, &sys->debug);
    chips_audio_callback_snapshot_onload(&im.audio.callback, &sys->audio.callback);
    mem_snapshot_onload(&im.mem, sys);
    *sys = im;
    return true;
}

#endif // CHIPS_IMPL
