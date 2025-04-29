#include "audio.h"
#include "device.h"
#include "source.h"

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>

#include <miniaudio.h>
#include <fmt.h>

int main() {
    struct audio_context * context = init_audio();

    struct sound_source note_sound = init_source("test.wav");
    struct sound_source ost_sound_1 = init_source("suzume-ost.wav");
    
    source_set_volume(&note_sound, 25);
    // audio_add_sound(context, note_sound);

    source_set_volume(&ost_sound_1, 150);
    audio_add_sound(context, ost_sound_1);

    device_toggle_play(context->device); // is paused as default

    char key = 0;

    while (key != 'q') { 
        // system("clear");
        printf("Default device: %s\n", context->device->info.name);
        puts("Options |>");
        puts("P: Pause/Play");
        puts("Q: Quit");

        printf("> ");
        key = tolower(getchar());
        puts("");

        switch (key) {
            case 'p':
                device_toggle_play(context->device); break;
        }
    }
}
