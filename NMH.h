#include <vector>
#include <cstdint>
#include <stdexcept>
#include <iostream>
#include <fstream>
#include <filesystem>

// Special thanks to Venomalia for the CMPR to DXT1 code!
// https://github.com/Venomalia/DolphinTextureExtraction-tool/blob/82fc1f8fef505ac646f09e13887efa187fae0a9d/lib/AuroraLip/Texture/ImageEX.cs#L258

#pragma pack(push, 1)
struct DDS_PIXELFORMAT {
    uint32_t size = 32;
    uint32_t flags = 0x4; // DDPF_FOURCC
    uint32_t fourCC = 0x31545844; // 'DXT1' in little-endian
    uint32_t rgbBitCount = 0;
    uint32_t rBitMask = 0;
    uint32_t gBitMask = 0;
    uint32_t bBitMask = 0;
    uint32_t aBitMask = 0;
};

struct DDS_HEADER {
    uint32_t magic = 0x20534444; // 'DDS ' in little-endian
    uint32_t size = 124;
    uint32_t flags = 0x0002100F; // DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH | DDSD_PIXELFORMAT | DDSD_LINEARSIZE
    uint32_t height;
    uint32_t width;
    uint32_t pitchOrLinearSize; // (width * height) / 2 for DXT1
    uint32_t depth = 0;
    uint32_t mipMapCount = 0;
    uint32_t reserved1[11] = { 0 };
    DDS_PIXELFORMAT pixelFormat;
    uint32_t caps = 0x1000; // DDSCAPS_TEXTURE
    uint32_t caps2 = 0;
    uint32_t caps3 = 0;
    uint32_t caps4 = 0;
    uint32_t reserved2 = 0;
};
#pragma pack(pop)

uint8_t Swap(uint8_t value)
{
    return (uint8_t)((unsigned long long)((value * 8623620610L) & 0x10884422010L) % 1023uL);
}

uint16_t Swap16(uint16_t value)
{
    return (value >> 8) | (value << 8);
}

uint8_t SwapAlternateBits(uint8_t value)
{
    return (uint8_t)((value & 0xAA) >> 1 | (value & 0x55) << 1);
}

// Decode function
void decodeBlock(std::vector<uint64_t>& data, int blockWidth, int blockHeight)
{
    std::vector<uint64_t> linebuffer(blockWidth * 2);

    for (int i = 0; i < blockHeight; i += 2)
    {
        int pos = 0;
        std::memcpy(linebuffer.data(), &data[i * blockWidth], linebuffer.size() * sizeof(uint64_t));

        for (int bi = 0; bi < blockWidth; bi += 2)
        {
            data[i * blockWidth + bi] = linebuffer[pos++];
            data[i * blockWidth + bi + 1] = linebuffer[pos++];
            data[(i + 1) * blockWidth + bi] = linebuffer[pos++];
            data[(i + 1) * blockWidth + bi + 1] = linebuffer[pos++];
        }
    }
}


// Swaps CMPR colors and fixes endianess
void swapCMPRColors(std::vector<uint8_t>& data)
{
    size_t dataSize = data.size();

    uint16_t* data16 = reinterpret_cast<uint16_t*>(data.data());

    size_t numElements = dataSize / sizeof(uint16_t);

    for (size_t i = 0; i < numElements; i += 4)
    {
        data16[i] = Swap16(data16[i]);
        data16[i + 1] = Swap16(data16[i + 1]);

        uint8_t byte0 = SwapAlternateBits(data[(i + 2) * 2 + 0]);
        uint8_t byte1 = SwapAlternateBits(data[(i + 2) * 2 + 1]);
        uint8_t byte2 = SwapAlternateBits(data[(i + 2) * 2 + 2]);
        uint8_t byte3 = SwapAlternateBits(data[(i + 2) * 2 + 3]);

        uint8_t b0 = Swap(byte0);
        uint8_t b1 = Swap(byte1);
        uint8_t b2 = Swap(byte2);
        uint8_t b3 = Swap(byte3);

        data[(i + 2) * 2 + 0] = b0;
        data[(i + 2) * 2 + 1] = b1;
        data[(i + 2) * 2 + 2] = b2;
        data[(i + 2) * 2 + 3] = b3;
    }
}

// Converts CMPR to DXT1
void convertCMPRToDXT1(std::vector<uint8_t>& data, int width, int height)
{
    if (width % 4 != 0 || height % 4 != 0) return; // Ensure valid block size

    swapCMPRColors(data);

    size_t blockCount = data.size() / 8;
    std::vector<uint64_t> data64(blockCount);
    std::memcpy(data64.data(), data.data(), blockCount * sizeof(uint64_t));

    decodeBlock(data64, width / 4, height / 4);

    std::memcpy(data.data(), data64.data(), blockCount * sizeof(uint64_t));
}

void GCT0CMPRToDXT1DDS(const std::filesystem::path& path)
{
    const std::string out_string = path.stem().string() + ".dds";
    std::vector<unsigned char> buffer;
    std::vector<unsigned char> out_buf;

    // read file into buffer
    {
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file) 
        {
            throw std::runtime_error("Failed to open file: " + path.string());
        }

        std::streamsize size = file.tellg();
        file.seekg(0, std::ios::beg);

        buffer.resize(size);
        if (!file.read(reinterpret_cast<char*>(buffer.data()), size))
        {
            throw std::runtime_error("Failed to read file: " + path.string());
        }
    }

    auto buf_ptr = buffer.data();
    unsigned char image_type = *(buf_ptr + 7);
    unsigned char width_byte1 = *(buf_ptr + 8);
    unsigned char width_byte2 = *(buf_ptr + 9);
    unsigned char height_byte1 = *(buf_ptr + 10);
    unsigned char height_byte2 = *(buf_ptr + 11);

    uint16_t width = (static_cast<uint16_t>(width_byte1) << 8) | width_byte2;
    uint16_t height = (static_cast<uint16_t>(height_byte1) << 8) | height_byte2;

    if (image_type == 0x06)
    {
        return;
    }

    DDS_HEADER header;
    header.width = static_cast<uint32_t>(width);
    header.height = static_cast<uint32_t>(height);
    header.pitchOrLinearSize = (header.width * header.height) / 2;

    out_buf.insert(out_buf.end(), buf_ptr + 64, buf_ptr + 64 + header.pitchOrLinearSize);

    convertCMPRToDXT1(out_buf, width, height);

    std::ofstream out(out_string, std::ios::binary);
    if (!out)
    {
        throw std::runtime_error("Failed to open output file: " + out_string);
    }

    out.write(reinterpret_cast<char*>(&header), sizeof(DDS_HEADER));
    out.write(reinterpret_cast<char*>(out_buf.data()), out_buf.size());
    
    if (!out)
    {
        throw std::runtime_error("Failed to write output file: " + out_string);
    }
    
}