#define _CRT_SECURE_NO_WARNINGS
#include <iostream>
#include <vector>
#include <filesystem>
#include <string>

#include "extractorimpl.h"

namespace fs = std::filesystem;

// TODO: add --metadata mode functionality
// TODO: add RSL support
// TODO: add --compress mode (if possible)

int main(int argc, char* argv[])
{
    std::string mode;
    fs::path directory;

    std::cout << std::endl << std::endl << std::endl << std::endl; // im.. sorry
    std::cout << "DDSExtractor" << std::endl;
    std::cout << "------------" << std::endl;
    std::cout << "Supported file extensions: .bin, .dat, .sti, .jmb" << std::endl;
    std::cout << "In order to get access to such files, please use: https://github.com/Timo654/No-More-RSL" << std::endl;
    std::cout << std::endl;

    if ( argc < 2 )
    {
        std::cout << "Please specify the mode that the tool should run in (--extract --extracthashed --import --nmhfixandhash or --bintodds): ";
        std::getline( std::cin, mode );
    }
    else
    {
        mode = argv[1];
    }

    if ( mode != "--extract" && mode != "--extracthashed" && mode != "--import" && mode != "--nmhfixandhash" && mode != "--btole" && mode != "--extractarchive" && mode != "--gm2" && mode != "--bintodds")
    {
        std::cerr << "Invalid mode: " << mode << std::endl;
        std::cerr << "Mode flag should be either --extract to extract DDS files, or --import to re-import DDS files" << std::endl;
        return 1;
    }

    if ( argc < 3 )
    {
        std::cout << "Please specify the path you want the tool to work in: ";
        std::string input_dir;
        std::getline( std::cin, input_dir );
        directory = fs::path( input_dir );
    }
    else
    {
        directory = argv[2];
    }

    ExtractorMode extractor_mode_flag = DDSExtractor::GetModeFromString( mode );
    if ( extractor_mode_flag == ExtractorMode::NONE ) 
    {
        std::cerr << "Invalid mode: " << mode << std::endl;
        std::cerr << "Mode flag should be one of --extract, --extracthashed, --import, --nmhfixandhash or --bintodds" << std::endl;
        return 1;
    }

    std::vector<std::string> extensions = { ".bin", ".BIN", ".dat", ".DAT", ".sti", ".STI", ".jmb", ".JMB", ".GM2" };

    DDSExtractor::ProcessDirectory( directory, extensions, extractor_mode_flag );

    return 0;
}