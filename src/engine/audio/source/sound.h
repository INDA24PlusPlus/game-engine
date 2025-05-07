#pragma once
#include <fstream>
#include <miniaudio.h>

#include "engine/core.h"

#define MAX_SOUND_BUF_SIZE 50 * 8 * 1024 // 50KB
#define SOUND_BUF_TYPE i16
#define PCMS16LE // remove this if SOUND_BUF_TYPE is no longer i16

static void data_callback(ma_device *pDevice, void *pOutput, const void *pInput, ma_uint32 frameCount) {

}
