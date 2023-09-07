#define CHIPS_IMPL
#include "chips/chips_common.h"
#include "common.h"
#include "chips/m6502.h"
#include "chips/m6522.h"
#include "chips/m6561.h"
#include "chips/kbd.h"
#include "chips/mem.h"
#include "chips/clk.h"
#include "systems/c1530.h"
#include "systems/vic20.h"
#include "roms/vic20-roms.h"

vic20_desc_t vic20_desc(vic20_joystick_type_t joy_type, vic20_memory_config_t mem_config, bool c1530_enabled) {
    return (vic20_desc_t) {
        .c1530_enabled = c1530_enabled,
        .joystick_type = joy_type,
        .mem_config = mem_config,
        .roms = {
            .chars = { .ptr=dump_vic20_characters_901460_03_bin, .size=sizeof(dump_vic20_characters_901460_03_bin) },
            .basic = { .ptr=dump_vic20_basic_901486_01_bin, .size=sizeof(dump_vic20_basic_901486_01_bin) },
            .kernal = { .ptr=dump_vic20_kernal_901486_07_bin, .size=sizeof(dump_vic20_kernal_901486_07_bin) },
        },
    };
}

int main() {
    vic20_joystick_type_t joy_type = VIC20_JOYSTICKTYPE_NONE;
    vic20_memory_config_t mem_config = VIC20_MEMCONFIG_STANDARD;
    bool c1530_enabled = false;
    vic20_desc_t desc = vic20_desc(joy_type, mem_config, c1530_enabled);
    vic20_t vic20;
    vic20_init(&vic20, &desc);
    while (1) {
        int ticks = vic20_exec(&vic20, vic20.pins);
        printf("Tick: %d\n", ticks);
    }
}