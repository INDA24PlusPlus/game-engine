#include "audio.h"
#include "device.h"
#include "effects/reverb.h"
#include "source.h"
#include "timer.h"
#include "vec.h"

#include <ctype.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>

#include <miniaudio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include <pthread.h>

char is_spinning;

void * spin_around(void * arg);
void clear_line_stdin();

int main() {
    struct audio_context * context = init_audio();

    struct sound_source note_sound = init_source("assets/test.wav");
    struct sound_source ost_sound = init_source("assets/suzume-ost.wav");
    struct sound_source water_sound = init_source("assets/water-stream.wav");
    
    // source_set_volume(&note_sound, 25);
    // audio_add_sound(context, note_sound);

    // source_set_volume(&ost_sound, 100);
    // audio_add_sound(context, ost_sound);

    source_set_volume(&water_sound, 100);
    audio_add_sound(context, water_sound);

    device_toggle_play(context->device); // is paused as default

    char key = 0;

    struct reverb_effect * reverb = &context->sources.sources[0].reverb;
    pthread_t spin_thread;

    while (key != 'q') { 
        // system("clear");
        printf("Default device: %s\n", context->device->info.name);
        puts("Options |>");
        puts("P: Pause/Play");
        puts("R: Reverb (On/Off)");
        puts("S: Spin");
        puts("Q: Quit");

        printf("> ");
        key = tolower(getchar());
        clear_line_stdin();

        switch (key) {
            case 'p':
                device_toggle_play(context->device); break;
            case 'r':
                reverb->enabled ^= 1; break;
            case 's':
                is_spinning = 1;
                if (pthread_create(&spin_thread, NULL, &spin_around, context) != 0) {
                    puts("Failed to initialize spin thread!");
                    is_spinning = 0;
                    break;
                }
                puts("Press enter to stop spinning!");
                fflush(stdout);
                fflush(stdin);
                while ((key = getchar()) != '\n' && key != EOF) {
                    printf("key=%c\n", key);
                }
                printf("key=%d\n", key);
                is_spinning = 0;
                if (pthread_join(spin_thread, NULL) != 0) {
                    puts("Failed to join spin thread");
                }
                break;
        }
    }
}


void * spin_around(void * arg) {
    struct audio_context * context = arg;
    double theta = 0.0, theta_delta = (double)M_PI / (double)64.0;

    struct timespec remaining, request = {.tv_nsec = 60*1000*1000, .tv_sec = 0}; // sleep for 60ms

    while (is_spinning) {
        float x = cosf(theta), y = sinf(theta);

        // printf("%.3f: [%.3f, %.3f]\n", theta, x, y);
        device_set_position(context->device, vec3(x, 0.0f, y));

        if (nanosleep(&request, &remaining) != 0) {
            puts("You are an insomniac");
            return NULL;
        }
        theta += theta_delta;
    }


    return NULL;
}

void clear_line_stdin() {
    char c;
    while (c = getchar(), c != '\n' && c != EOF);
}
