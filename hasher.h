#ifndef HASHER_H
#define HASHER_H

#include "inc_wrapper.h"

// implementation from: https://web.archive.org/web/20230319040222/https://gist.github.com/SutandoTsukai181/dfe6884ee1254791ab166a0e876dda39
// credit to SutandoTsukai181

namespace hasher
{
    int swapEndian32(char* value)
    {
        return ((value[0] & 0xFF) << 24)
            | ((value[1] & 0xFF) << 16)
            | ((value[2] & 0xFF) << 8)
            | (value[3] & 0xFF);
    }

    int swapEndian16(char* value)
    {
        return ((value[0] & 0xFF) << 8)
            | (value[1] & 0xFF);
    }

    int rotateLeft32(uint32_t value, uint8_t count)
    {
        return (value << count) | (value >> (32 - count));
    }

    uint32_t DWORDInHexBytesToU32(const char* value)
    {
        // this is little endian so this might be a problem for fucking sti
        uint32_t decimal =
            (value[0]) |   // least significant byte
            (value[1] << 8) |   // second byte
            (value[2] << 16) |   // third byte
            (value[3] << 24);    // most significant byte

        return decimal;
    }

    uint16_t WORDInHexBytesToU16(const char* value)
    {
        uint16_t decimal =
            (value[0]) |        // least significant byte
            (value[1] << 8);    // most significant byte

        return decimal;
    }

    std::string CalculateHashOriginal(const char* path)
    {
        std::cout << std::endl << "No More Hashes v1.1 by SutandoTsukai181" << std::endl << std::endl;

        std::ifstream file(path, std::ios::binary | std::ios::ate);

        if (!file.is_open())
        {
            throw std::exception("Error: File could not be opened");
        }

        int start = 0;
        int size = file.tellg();

        char* magic = new char[5];
        char* int_val = new char[4];

        uint16_t width = 0;
        uint16_t height = 0;

        file.seekg(0, std::ios::beg);
        file.read(magic, 4);
        magic[4] = '\0';

        // GCT0 header can either have GCT0 or null as a magic
        if ((!strcmp(magic, "GCT0") || *magic == 0) && size > 0x40)
        {
            std::cout << "Reading GCT0 header...\n";

            bool bigEndian = !strcmp(magic, "GCT0");

            // Get width/height to generate full replacement texture name
            file.seekg(8, std::ios::beg);
            file.read(int_val, 2);
            width = bigEndian ? swapEndian16(int_val) : *(uint16_t*)int_val;
            file.read(int_val, 2);
            height = bigEndian ? swapEndian16(int_val) : *(uint16_t*)int_val;

            // Try checking for the texture start (should be always 0x40)
            file.seekg(0x10, std::ios::beg);
            file.read(int_val, 4);
            start = bigEndian ? swapEndian32(int_val) : *(int*)int_val;

            if (start != 0x40)
            {
                std::cout << "Header is invalid. Hashing the whole file...\n";

                // This turned out to be a non valid header - revert back to default values
                start = 0;
                width = 0;
                height = 0;
            }
            else
            {
                std::cout << "Successfully read the header. Hashing the texture data...\n";

                // Remove header size from the texture size
                size -= start;

                file.seekg(start, std::ios::beg);
                file.read(magic, 4);
                magic[4] = '\0';

                if (!strcmp(magic, "K7TX"))
                {
                    std::cout << "Reading K7TX header...\n";

                    // DDS header starts right after K7TX
                    // We're assuming this is always little endian
                    start += 8;
                    file.read((char*)&size, 4);
                }
            }
        }
        else
        {
            std::cout << "Could not find a GCT0 or K7TX header. Hashing the whole file...\n";
        }

        std::cout << "\n";

        // Read the texture to a buffer
        int sizeAligned = size / 4;
        int chunkSize = std::max(sizeAligned / 0x40, 1);

        int* buffer = new int[sizeAligned];
        file.seekg(start, std::ios::beg);
        file.read((char*)buffer, size);

        // Calculate the hash

        // Initial value
        uint32_t hash = 0xDEADBEEF;

        uint32_t index = 0;
        while (index < sizeAligned)
        {
            hash = (rotateLeft32(hash ^ (rotateLeft32(buffer[index] * 0xCC9E2D51, 15) * 0x1B873593), 13) + 0xFADDAF14) * 5;
            index += chunkSize;
        }

        // Read the remaining 1-3 bytes, if any
        char extra;
        uint32_t extra_val = 0;
        switch (size & 3)
        {
        case 3:
            file.seekg(start + (sizeAligned * 4) + 2, std::ios::beg);
            file.read(&extra, 1);
            extra_val = extra << 16;
            // Fallthrough
        case 2:
            file.seekg(start + (sizeAligned * 4) + 1, std::ios::beg);
            file.read(&extra, 1);
            extra_val ^= extra << 8;
            // Fallthrough
        case 1:
            file.seekg(start + (sizeAligned * 4), std::ios::beg);
            file.read(&extra, 1);
            extra_val = rotateLeft32((extra_val ^ extra) * 0xCC9E2D51, 15) * 0x1B873593;
            hash ^= extra_val;
        default:
            break;
        }

        hash ^= size;

        hash = ((hash >> 16) ^ hash) * 0x85EBCA6B;
        hash = ((hash >> 13) ^ hash) * 0xC2B2AE35;
        hash = (hash >> 16) ^ hash;

        if (width > 0 && height > 0 && width < 10000 && height < 10000)
        {
            char name[18 + 1];
            sprintf_s(name, "%04dx%04d_%x", width, height, hash);

            std::cout << "Full texture name: " << name << "\n\n";
            return name;
        }

        return {};
    }

    // broken
    std::string calculateHash(const char* path)
    {
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file.is_open())
        {
            throw std::runtime_error("Error: File could not be opened");
        }

        int size = file.tellg();
        char* buffer = new char[size];
        file.seekg(0, std::ios::beg);
        file.read(buffer, size);

        int start = 0;
        int sti_texture_start = 0;
        uint16_t width = 0;
        uint16_t height = 0;
        bool bigEndian = false;
        bool hasHeader = false;
        bool sti = false;

        std::string filePath(path);
        std::string extension = filePath.substr(filePath.find_last_of('.'));

        if (extension == ".bin" || extension == ".BIN")
        {
            // no additional search needed; the code already works for .bin format
            hasHeader = true;
            start = 0;
        }
        else if (extension == ".jmb" || extension == ".JMB")
        {
            for (int i = 0; i < size - 8; ++i)
            {
                if (std::memcmp(buffer + i, "\x00\x00\x00\x00\x06\x00\x00\x00", 8) == 0)
                {
                    std::cout << "Found JMB texture header at position " << i << ".\n";
                    hasHeader = true;
                    start = i;
                    break;
                }
            }
        }
        else if (extension == ".sti" || extension == ".STI")
        {
            sti = true;
            for (int i = 0; i < size - 4; ++i)
            {
                if (std::memcmp(buffer + i, "GCT0", 4) == 0)
                {
                    std::cout << "Found STI header (GCT0) at position " << i << ".\n";
                    hasHeader = true;
                    bigEndian = true;
                    width = WORDInHexBytesToU16(buffer + i + 8);
                    height = WORDInHexBytesToU16(buffer + i + 10);
                    sti_texture_start = *(int*)(buffer + i + 16);
                    start = i;
                    break;
                }
            }
        }

        if (hasHeader && size > start + 0x40)
        {
            if (!sti)
            {
                width = bigEndian ? swapEndian16(buffer + start + 8) : *(uint16_t*)(buffer + start + 8);
                height = bigEndian ? swapEndian16(buffer + start + 10) : *(uint16_t*)(buffer + start + 10);
            }

            int textureStart = bigEndian ? swapEndian32(buffer + start + 0x10) : *(int*)(buffer + start + 0x10);
            if (sti)
            {
                textureStart = sti_texture_start;
            }
            if (textureStart != 0x40)
            {
                std::cout << "Header is invalid. Hashing the whole file...\n";
                start = 0;
                width = 0;
                height = 0;
            }
            else
            {
                std::cout << "Successfully read the header. Hashing the texture data...\n";
                size -= textureStart;
                start += textureStart;

                // check for "K7TX" magic byte header at the texture start position
                if (strncmp(buffer + start, "K7TX", 4) == 0)
                {
                    std::cout << "Found K7TX header.\n";
                    start += 8;
                    size = bigEndian ? swapEndian16(buffer + start - 4) : *(int*)(buffer + start - 4);
                }
            }
        }
        else
        {
            std::cout << "Could not find a GCT0 or JMB-specific header. Hashing the whole file...\n";
        }

        std::cout << "\n";

        int sizeAligned = size / 4;
        int chunkSize = std::max(sizeAligned / 0x40, 1);
        std::string textureData_string = buffer + start;
        int* textureData = reinterpret_cast<int*>(buffer + start);

        uint32_t hash = 0xDEADBEEF;
        uint32_t index = 0;

        while (index < sizeAligned)
        {
            int currentPos = start + index * 4;
            if (currentPos >= size) {
                break;  // Ensure we donÅft read past the end of the buffer
            }

            int textureValue = bigEndian
                ? swapEndian32(buffer + currentPos) // Swap endianness if needed
                : *(reinterpret_cast<int*>(buffer + currentPos)); // Directly interpret bytes as an int

            // Update hash
            hash = (rotateLeft32(hash ^ (rotateLeft32(textureValue * 0xCC9E2D51, 15) * 0x1B873593), 13) + 0xFADDAF14) * 5;

            // Increment index
            index += chunkSize;
        }

        uint32_t extra_val = 0;
        for (int i = 0; i < (size & 3); ++i)
        {
            extra_val |= buffer[start + (sizeAligned * 4) + i] << (8 * i);
        }
        if (extra_val)
        {
            extra_val = rotateLeft32((extra_val) * 0xCC9E2D51, 15) * 0x1B873593;
            hash ^= extra_val;
        }

        hash ^= size;
        hash = ((hash >> 16) ^ hash) * 0x85EBCA6B;
        hash = ((hash >> 13) ^ hash) * 0xC2B2AE35;
        hash = (hash >> 16) ^ hash;

        // format the final hash as "<width>x<height>_<hash>" for Replacement folder
        if (width > 0 && height > 0 && width < 10000 && height < 10000)
        {
            char name[18 + 1];
            sprintf_s(name, "%04dx%04d_%x", width, height, hash);
            delete[] buffer;
            return name;
        }

        delete[] buffer;
        return {};
    }

}

#endif
