#include "game/navigation/SectorNavigationCompression.h"

#include <algorithm>
#include <cstring>
#include <limits>

namespace game {
namespace {

constexpr uint32_t LayerMagic = 0x3143564eu; // NVC1 in little endian.
constexpr uint32_t LayerVersion = 1;
constexpr int HeaderSize = 16;

uint32_t Checksum(const uint8_t* bytes, int count)
{
    uint32_t value = 2166136261u;
    for (int index = 0; index < count; ++index) {
        value ^= bytes[index];
        value *= 16777619u;
    }
    return value;
}

void WriteU32(uint8_t* output, uint32_t value)
{
    output[0] = static_cast<uint8_t>(value);
    output[1] = static_cast<uint8_t>(value >> 8u);
    output[2] = static_cast<uint8_t>(value >> 16u);
    output[3] = static_cast<uint8_t>(value >> 24u);
}

uint32_t ReadU32(const uint8_t* input)
{
    return static_cast<uint32_t>(input[0])
            | static_cast<uint32_t>(input[1]) << 8u
            | static_cast<uint32_t>(input[2]) << 16u
            | static_cast<uint32_t>(input[3]) << 24u;
}

int RunLength(const uint8_t* input, int size, int start)
{
    int count = 1;
    while (start + count < size && count < 128
           && input[start + count] == input[start]) {
        ++count;
    }
    return count;
}

} // namespace

int SectorNavigationMaximumCompressedLayerSize(int uncompressedSize)
{
    if (uncompressedSize < 0
        || uncompressedSize > std::numeric_limits<int>::max() - HeaderSize - 1) {
        return 0;
    }
    return HeaderSize + uncompressedSize + (uncompressedSize + 127) / 128;
}

bool CompressSectorNavigationLayer(
        const uint8_t* input,
        int inputSize,
        uint8_t* output,
        int outputCapacity,
        int& outputSize)
{
    outputSize = 0;
    const int maximumSize = SectorNavigationMaximumCompressedLayerSize(inputSize);
    if (input == nullptr || output == nullptr || inputSize < 0
        || maximumSize == 0 || outputCapacity < maximumSize) {
        return false;
    }
    WriteU32(output, LayerMagic);
    WriteU32(output + 4, LayerVersion);
    WriteU32(output + 8, static_cast<uint32_t>(inputSize));
    WriteU32(output + 12, Checksum(input, inputSize));
    int source = 0;
    int destination = HeaderSize;
    while (source < inputSize) {
        const int run = RunLength(input, inputSize, source);
        if (run >= 4) {
            output[destination++] = static_cast<uint8_t>(0x80u | (run - 1));
            output[destination++] = input[source];
            source += run;
            continue;
        }
        const int literalStart = source;
        source += run;
        while (source < inputSize && source - literalStart < 128) {
            const int nextRun = RunLength(input, inputSize, source);
            if (nextRun >= 4) break;
            source += std::min(nextRun, 128 - (source - literalStart));
        }
        const int literalCount = source - literalStart;
        output[destination++] = static_cast<uint8_t>(literalCount - 1);
        std::memcpy(output + destination, input + literalStart, literalCount);
        destination += literalCount;
    }
    outputSize = destination;
    return true;
}

bool DecompressSectorNavigationLayer(
        const uint8_t* input,
        int inputSize,
        uint8_t* output,
        int outputCapacity,
        int& outputSize)
{
    outputSize = 0;
    if (input == nullptr || output == nullptr || inputSize < HeaderSize) return false;
    if (ReadU32(input) != LayerMagic || ReadU32(input + 4) != LayerVersion) return false;
    const uint32_t rawSize = ReadU32(input + 8);
    if (rawSize > static_cast<uint32_t>(outputCapacity)
        || rawSize > static_cast<uint32_t>(std::numeric_limits<int>::max())) {
        return false;
    }
    int source = HeaderSize;
    int destination = 0;
    while (source < inputSize && destination < static_cast<int>(rawSize)) {
        const uint8_t tag = input[source++];
        const int count = (tag & 0x7f) + 1;
        if (destination + count > static_cast<int>(rawSize)) return false;
        if ((tag & 0x80u) != 0) {
            if (source >= inputSize) return false;
            std::memset(output + destination, input[source++], count);
        } else {
            if (source + count > inputSize) return false;
            std::memcpy(output + destination, input + source, count);
            source += count;
        }
        destination += count;
    }
    if (source != inputSize || destination != static_cast<int>(rawSize)
        || Checksum(output, destination) != ReadU32(input + 12)) {
        return false;
    }
    outputSize = destination;
    return true;
}

} // namespace game
