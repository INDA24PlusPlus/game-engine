#include "signature.hpp"

Signature set_signature(Signature signature, u32 component_id) {
    return signature | (std::bitset<MAX_COMPONENTS>(1) << component_id);
}

Signature remove_signature(Signature signature, u32 component_id) {
    return signature & ~(std::bitset<MAX_COMPONENTS>(1) << component_id);
}
