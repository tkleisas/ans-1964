#pragma once

#include <juce_core/juce_core.h>
#include <cstdint>
#include <vector>

namespace ansvst {

inline bool parseScoreBytes(const void* raw, size_t size,
                            std::vector<uint8_t>& out, int& width, int& px)
{
    if (raw == nullptr || size < 16) return false;
    const auto* b = static_cast<const uint8_t*>(raw);
    if (b[0] != 'A' || b[1] != 'N' || b[2] != 'S' || b[3] != '1') return false;
    const auto rd16 = [](const uint8_t* p)
    {
        return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
    };
    const auto rd32 = [](const uint8_t* p)
    {
        return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
               ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
    };
    const uint16_t ver = rd16(b + 4);
    const uint16_t w = rd16(b + 6);
    const uint16_t h = rd16(b + 8);
    const uint16_t ppm = rd16(b + 10);
    const uint32_t bytes = rd32(b + 12);
    if (ver != 1 || h != 720 || w == 0 || w > 8192) return false;
    if (bytes != (uint32_t)w * 720u || size < 16u + (size_t)bytes) return false;
    out.assign(b + 16, b + 16 + (size_t)bytes);
    width = w;
    px = ppm > 0 ? ppm : 10;
    return true;
}

inline bool loadScoreFile(const juce::File& file,
                          std::vector<uint8_t>& out, int& width, int& px)
{
    juce::FileInputStream in(file);
    if (!in.openedOk()) return false;
    const auto len = in.getTotalLength();
    if (len <= 16 || len > 64 * 1024 * 1024) return false;
    std::vector<uint8_t> raw((size_t)len);
    if (in.read(raw.data(), (int)raw.size()) != (int)raw.size()) return false;
    return parseScoreBytes(raw.data(), raw.size(), out, width, px);
}

inline bool saveScoreFile(const juce::File& file, const uint8_t* data, int width, int px)
{
    if (data == nullptr || width <= 0 || width > 8192) return false;
    const uint32_t bytes = (uint32_t)width * 720u;
    uint8_t hdr[16] = {};
    hdr[0] = 'A'; hdr[1] = 'N'; hdr[2] = 'S'; hdr[3] = '1';
    hdr[4] = 1; hdr[5] = 0;
    hdr[6] = (uint8_t)(width & 0xff); hdr[7] = (uint8_t)(width >> 8);
    hdr[8] = 0xd0; hdr[9] = 0x02; // 720
    hdr[10] = (uint8_t)(px & 0xff); hdr[11] = (uint8_t)(px >> 8);
    hdr[12] = (uint8_t)(bytes & 0xff); hdr[13] = (uint8_t)((bytes >> 8) & 0xff);
    hdr[14] = (uint8_t)((bytes >> 16) & 0xff); hdr[15] = (uint8_t)((bytes >> 24) & 0xff);

    file.deleteFile();
    juce::FileOutputStream out(file);
    if (!out.openedOk()) return false;
    out.write(hdr, sizeof(hdr));
    out.write(data, (size_t)bytes);
    out.flush();
    return out.getStatus().wasOk();
}

} // namespace ansvst
