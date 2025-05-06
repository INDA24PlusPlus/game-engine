#pragma  once
#include "engine/utils/logging.h"
#include "types.h"

const u32 MAX_RESOURCES = 128;

class ResourceBase  {
    protected:
        static u32 id_counter;
    public:
        static u32 get_id();

        ResourceBase() {
        }
};

template <typename T>
class Resource: public ResourceBase {
    public:
        static u32 get_id() {
            static u32 id = id_counter++;
            if (id >= MAX_RESOURCES) {
                ERROR("Resource ID exceeds maximum resources");
            }
            return id;
        }
};

class ResourceManager {
    private:
        ResourceBase *resources[MAX_RESOURCES];
        u32 resource_count;
    public:
        ResourceManager();

        ~ResourceManager();

        template <typename T>
        T* register_resource(T* resource) {
            const u32 resource_id = T::get_id();
            resources[resource_id] = resource;
            resource_count++;
            return (T*)resources[resource_id];
        }

        template <typename T>
        T* get_resource() {
            const u32 resource_id = T::get_id();
            return (T*)resources[resource_id];
        }
};
