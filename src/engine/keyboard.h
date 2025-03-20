#ifndef _KEYBOARD_H
#define _KEYBOARD_H

#include "platform.h"

namespace engine {

const u64 shift_width = 6;

struct Keyboard {
    // 256 states.
    u64 keys_down[4];

    void reset() {
        keys_down[0] = 0;
        keys_down[1] = 0;
        keys_down[2] = 0;
        keys_down[3] = 0;
    }

    void register_key(platform::KeyEvent event) {
        keys_down[event.virtual_key >> shift_width] |= (1ull << (event.virtual_key & 0x3F));
    }

    void unregister_key(platform::KeyEvent event) {
        keys_down[event.virtual_key >> shift_width] &= ~(1ull << (event.virtual_key & 0x3F));
    }

    void toggle(platform::KeyEvent event) {
        keys_down[event.virtual_key >> shift_width] ^= (1ull << (event.virtual_key & 0x3F));
    }

    bool is_key_down(char key) {
        return (keys_down[key >> shift_width] & (1ull << (key & 0x3F))) != 0;
    }
};
}

#endif
