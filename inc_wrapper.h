#pragma once

#include <iostream>
#include <fstream>
#include <vector>
#include <filesystem>
#include <string>
#include <algorithm> 

#ifdef __GNUC__
#include <cstring>
#define sprintf_s(buf, ...) snprintf((buf), sizeof(buf), __VA_ARGS__)
#endif

using u8 = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;

namespace fs = std::filesystem;