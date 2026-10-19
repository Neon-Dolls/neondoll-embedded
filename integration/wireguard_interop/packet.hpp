// Test-only framing, not a NeonDoll protocol or cryptographic construction.
#ifndef ND_INTEROP_PACKET_HPP
#define ND_INTEROP_PACKET_HPP
#include <cstddef>
#include <cstdint>
#include <cstring>
namespace interop {
constexpr size_t header_size = 14, max_payload = 1372, oversize_payload = 1373;
inline bool make(uint8_t* out, size_t size, char kind, char origin, uint32_t sequence) {
    if (size < header_size || size > oversize_payload ||
        (kind != 'Q' && kind != 'E') || (origin != 'D' && origin != 'L')) return false;
    std::memcpy(out, "NDWG32C!", 8); out[8] = kind; out[9] = origin;
    for (unsigned i = 0; i < 4; ++i) out[10+i] = uint8_t(sequence >> (24-8*i));
    for (size_t i = header_size; i < size; ++i) out[i] = uint8_t(sequence+i);
    return true;
}
inline bool parse(const uint8_t* in, size_t size, uint32_t& sequence) {
    if (size < header_size || size > oversize_payload || std::memcmp(in,"NDWG32C!",8) ||
        (in[8] != 'Q' && in[8] != 'E') || (in[9] != 'D' && in[9] != 'L')) return false;
    sequence = 0;
    for (unsigned i = 0; i < 4; ++i) sequence = (sequence << 8) | in[10+i];
    for (size_t i = header_size; i < size; ++i) if (in[i] != uint8_t(sequence+i)) return false;
    return true;
}
}
#endif
