#ifndef EXTRACTORIMPL_H
#define EXTRACTORIMPL_H

#include "inc_wrapper.h"
#include "hasher.h"

const std::vector<uint8_t> DDS_MAGIC_PATTERN = { 0x44, 0x44, 0x53, 0x20, 0x7C };

enum class ExtractorMode
{
    EXTRACT,
    EXTRACT_HASHED,
    IMPORT,
    METADATA,
    // COMPRESS,
    NONE
};

namespace DDSExtractor
{
    /// <summary>
    /// Returns the extraction mode that the tool should run in based on the user's string input
    /// </summary>
    ExtractorMode GetModeFromString(const std::string& mode_string)
    {
        if ( mode_string == "--extract" ) return ExtractorMode::EXTRACT;
        if ( mode_string == "--extracthashed" ) return ExtractorMode::EXTRACT_HASHED;
        if ( mode_string == "--import" ) return ExtractorMode::IMPORT;
        if ( mode_string == "--metadata" ) return ExtractorMode::METADATA;

        return ExtractorMode::NONE;
    }

    /// <summary>
    /// This function finds the DDS magic bytes pattern in the file that it's given.
    /// </summary>
    bool FindPattern(std::istream& file, std::streampos& found_pos)
    {
        std::vector<u8> buffer( 1024 );
        const size_t pattern_length = DDS_MAGIC_PATTERN.size();
        std::vector<u8> window( pattern_length );

        found_pos = 0;

        while ( file.read( reinterpret_cast<char*>( buffer.data() ), buffer.size() ) || file.gcount() > 0 )
        {
            std::streamsize bytes_read = file.gcount();

            for ( std::streamsize i = 0; i < bytes_read; ++i )
            {
                std::rotate( window.begin(), window.begin() + 1, window.end() );
                window.back() = buffer[i];

                if ( window == DDS_MAGIC_PATTERN )
                {
                    found_pos = static_cast<std::streamoff>( file.tellg() ) - bytes_read + i - pattern_length + 1;
                    return true;
                }
            }
        }

        return false;
    }

    /// <summary>
    /// This function extracts the DDS data into a new file, the filename being the original + the suffix "_extracted", + of course the file extension ".dds"
    /// </summary>
    void ExtractDDS(const fs::path& file_path, std::streampos start)
    {
        std::ifstream file( file_path, std::ios::binary );

        file.seekg( start );

        std::vector<u8> dds_data( ( std::istreambuf_iterator<char>( file ) ),
                                    std::istreambuf_iterator<char>() );

        fs::path output_file_path = file_path.parent_path() / ( file_path.stem().string() + "_extracted.dds" );

        std::ofstream outputFile( output_file_path, std::ios::binary );
        if ( outputFile )
        {
            outputFile.write( reinterpret_cast<const char*>( dds_data.data() ), dds_data.size() );
            std::cout << "Extracted DDS data to: " << output_file_path << std::endl;
        }
    }

    void ExtractDDSHashed(const fs::path& file_path, std::streampos start)
    {
        std::ifstream file(file_path, std::ios::binary);

        file.seekg(start);

        std::vector<u8> dds_data( ( std::istreambuf_iterator<char>( file ) ),
                                    std::istreambuf_iterator<char>()) ;

        fs::path output_file_path = file_path.parent_path() / ( hasher::calculateHash( file_path.string().c_str() ) + ".dds" );

        std::ofstream outputFile(output_file_path, std::ios::binary);
        if (outputFile)
        {
            outputFile.write( reinterpret_cast<const char*>( dds_data.data() ), dds_data.size() );
            std::cout << "Extracted DDS data to: " << output_file_path << std::endl;
        }
    }

    /// <summary>
    /// This function re-imports DDS data (from a file, e.g. st00_extracted.dds) into its original file (in this case, it would be st00.BIN)
    /// </summary>
    void ImportDDS(const fs::path& original_file_path, const fs::path& dds_file_path)
    {
        std::fstream original_file( original_file_path, std::ios::in | std::ios::out | std::ios::binary );
        if ( !original_file )
        {
            std::cerr << "Error opening file: " << original_file_path << std::endl;
            return;
        }

        std::streampos found_pos;
        if ( !FindPattern( original_file, found_pos ) )
        {
            std::cerr << "DDS pattern not found in original file: " << original_file_path << std::endl;
            return;
        }

        std::ifstream dds_file( dds_file_path, std::ios::binary );
        if ( !dds_file )
        {
            std::cerr << "Error opening DDS file: " << dds_file_path << std::endl;
            return;
        }
        std::vector<u8> new_dds_data( ( std::istreambuf_iterator<char>( dds_file ) ),
                                        std::istreambuf_iterator<char>() );

        std::streamsize new_dds_size = new_dds_data.size();

        original_file.seekg( 0, std::ios::beg );
        std::vector<u8> data_before_dds_bytes( static_cast<size_t>( found_pos ) );
        original_file.read( reinterpret_cast<char*>( data_before_dds_bytes.data() ), found_pos );
        original_file.close();

        std::ofstream output_file( original_file_path, std::ios::binary | std::ios::trunc );
        if ( !output_file )
        {
            std::cerr << "Error reopening file for writing: " << original_file_path << std::endl;
            return;
        }

        output_file.write( reinterpret_cast<const char*>( data_before_dds_bytes.data() ), data_before_dds_bytes.size() );
        output_file.write( reinterpret_cast<const char*>( new_dds_data.data() ), new_dds_data.size() );

        original_file.open( original_file_path, std::ios::in | std::ios::binary );
        original_file.seekg( found_pos + new_dds_size );
        std::vector<u8> remaining_data( ( std::istreambuf_iterator<char>( original_file ) ),
                                          std::istreambuf_iterator<char>() );
        output_file.write( reinterpret_cast<const char*>( remaining_data.data() ), remaining_data.size() );

        std::cout << "Re-imported DDS data into: " << original_file_path << std::endl;
    }

    /// <summary>
    /// Once the program has been given a directory to work in, from the user, + the extraction mode, this function processes the given files and based on the extraction mode it does the necessary operation (extraction/reimport, etc.)
    /// </summary>
    void ProcessDirectory(const fs::path& directory, const std::vector<std::string>& extensions, ExtractorMode extract_mode)
    {
        for ( const auto& entry : fs::recursive_directory_iterator( directory ) )
        {
            if ( entry.is_regular_file() )
            {
                const auto& file_path = entry.path();
                if ( std::find( extensions.begin(), extensions.end(), file_path.extension().string() ) != extensions.end() )
                {
                    switch ( extract_mode )
                    {
                        case ExtractorMode::EXTRACT:
                        {
                            std::ifstream file( file_path, std::ios::binary );

                            if ( !file )
                            {
                                std::cerr << "Error opening file: " << file_path << std::endl;
                                continue;
                            }

                            std::streampos found_pos;
                            if ( FindPattern( file, found_pos ) )
                            {
                                std::cout << "DDS pattern found in file: " << file_path << " at position " << found_pos << std::endl;
                                ExtractDDS( file_path, found_pos );
                            }
                            else
                            {
                                std::cout << "DDS pattern not found in file: " << file_path << std::endl;
                            }
                            break;
                        }
                        case ExtractorMode::EXTRACT_HASHED:
                        {
                            std::ifstream file(file_path, std::ios::binary);

                            if (!file)
                            {
                                std::cerr << "Error opening file: " << file_path << std::endl;
                                continue;
                            }

                            std::streampos found_pos;
                            if (FindPattern(file, found_pos))
                            {
                                std::cout << "DDS pattern found in file: " << file_path << " at position " << found_pos << std::endl;
                                ExtractDDSHashed(file_path, found_pos);
                            }
                            else
                            {
                                std::cout << "DDS pattern not found in file: " << file_path << std::endl;
                            }
                            break;
                        }
                        case ExtractorMode::IMPORT:
                        {
                            fs::path dds_file_path = file_path.parent_path() / ( file_path.stem().string() + "_extracted.dds" );
                            if ( fs::exists( dds_file_path ) )
                            {
                                ImportDDS( file_path, dds_file_path );
                            }
                            break;
                        }
                        case ExtractorMode::METADATA:
                        {
                            std::cerr << "WIP" << std::endl;
                            break;
                        }
                        default:
                        {
                            std::cerr << "Unsupported mode." << std::endl;
                            break;
                        }
                    }
                }
            }
        }
    }
}

#endif