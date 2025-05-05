#include "types.h"
#include "resource.hpp"

u32 ResourceBase::id_counter = 0;

ResourceManager::ResourceManager() {
    resource_count = 0;
}

ResourceManager::~ResourceManager() {
    for (u32 i = 0; i < resource_count; i++) {
        delete resources[i];
    }
}
