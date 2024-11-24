#ifndef EXTRACTORIMPL_H
#define EXTRACTORIMPL_H

#include "inc_wrapper.h"
#include "hasher.h"

const std::vector<uint8_t> DDS_MAGIC_PATTERN = { 0x44, 0x44, 0x53, 0x20, 0x7C };

enum class ExtractorMode
{
    EXTRACT,
    EXTRACT_HASHED,
    EXTRACT_ARCHIVE,
    IMPORT,
    METADATA,
    NMH_FIX_AND_HASH,
    BIG_TO_LITTLE_ENDIAN,
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
        if ( mode_string == "--nmhfixandhash" ) return ExtractorMode::NMH_FIX_AND_HASH;
        if ( mode_string == "--btole" ) return ExtractorMode::BIG_TO_LITTLE_ENDIAN;

        return ExtractorMode::NONE;
    }

    // Helper function to reverse the byte order of data in-place
    void reverseBytes(char* data, std::size_t size)
    {
        std::reverse(data, data + size);
    }

    // Function to convert a big-endian binary file to little-endian
    bool convertBigEndianToLittleEndian(const fs::path& inputFilePath, const fs::path& outputFilePath) 
    {
        size_t wordSize = 4;
        // Open input file in binary read mode
        std::ifstream inputFile(inputFilePath, std::ios::binary);
        if (!inputFile) {
            std::cerr << "Error: Cannot open input file " << inputFilePath << std::endl;
            return false;
        }

        // Open output file in binary write mode
        std::ofstream outputFile(outputFilePath, std::ios::binary);
        if (!outputFile) {
            std::cerr << "Error: Cannot open output file " << outputFilePath << std::endl;
            return false;
        }

        // Process the file in chunks of `wordSize` bytes
        std::vector<char> buffer(wordSize);
        while (inputFile.read(buffer.data(), wordSize)) {
            // Reverse bytes in the buffer to convert endianness
            reverseBytes(buffer.data(), wordSize);
            // Write the reversed data to the output file
            outputFile.write(buffer.data(), wordSize);
        }

        // Handle the last chunk if the file size is not a multiple of wordSize
        if (inputFile.gcount() > 0) {
            buffer.resize(inputFile.gcount());
            reverseBytes(buffer.data(), buffer.size());
            outputFile.write(buffer.data(), buffer.size());
        }

        // Close the files
        inputFile.close();
        outputFile.close();

        return true;
    }

    // Helper function to read a block of bytes from a file
    std::vector<uint8_t> readBytes(std::ifstream& file, size_t numBytes) {
        std::vector<uint8_t> buffer(numBytes);
        file.read(reinterpret_cast<char*>(buffer.data()), numBytes);
        return buffer;
    }

    // Helper function to convert integers to a zero-padded string for filenames
    std::string intToFilename(int num) {
        std::ostringstream ss;
        ss << std::setw(3) << std::setfill('0') << num;
        return ss.str();
    }

    // Function to extract DDS files
    void ExtractTexturesFromArchive(const fs::path& filePath)
    {
        const std::vector<uint8_t> DDS_MAGIC = { 0x44, 0x44, 0x53, 0x20 }; // "DDS " magic bytes
        const size_t HEADER_SIZE = 72;
        const size_t DDS_HEADER_SIZE_OFFSET = 4;
        const size_t PIXEL_DATA_START_OFFSET = 128; // DDS header size is generally fixed at 128 bytes
        const std::vector<uint8_t> STOP_PATTERN = { 0x00, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00 };

        std::ifstream file(filePath, std::ios::binary);
        if (!file.is_open())
        {
            std::cerr << "Error: Could not open file: " << filePath << std::endl;
            return;
        }

        std::vector<uint8_t> buffer(HEADER_SIZE + DDS_MAGIC.size());
        int fileCount = 0;

        while (file.read(reinterpret_cast<char*>(buffer.data()), buffer.size())) 
        {
            for (size_t i = 0; i <= buffer.size() - DDS_MAGIC.size(); ++i)
            {
                if (std::equal(DDS_MAGIC.begin(), DDS_MAGIC.end(), buffer.begin() + i)) 
                {
                    // Seek back to include the 72 bytes before DDS magic
                    file.seekg(-static_cast<int>(buffer.size() - i + HEADER_SIZE), std::ios::cur);

                    // Read the DDS header and preceding 72 bytes
                    std::vector<uint8_t> fullHeader = readBytes(file, HEADER_SIZE + DDS_MAGIC.size());

                    // Get DDS header size to determine pixel data start point
                    uint32_t headerSize;
                    std::memcpy(&headerSize, fullHeader.data() + DDS_HEADER_SIZE_OFFSET, sizeof(uint32_t));

                    // Start reading pixel data until stop pattern is found
                    std::vector<uint8_t> pixelData;
                    std::vector<uint8_t> stopBuffer(STOP_PATTERN.size());
                    bool stopFound = false;

                    while (file.read(reinterpret_cast<char*>(stopBuffer.data()), stopBuffer.size()))
                    {
                        if (std::equal(STOP_PATTERN.begin(), STOP_PATTERN.end(), stopBuffer.begin())) 
                        {
                            stopFound = true;
                            file.seekg(-static_cast<int>(STOP_PATTERN.size()), std::ios::cur);
                            break;
                        }
                        pixelData.insert(pixelData.end(), stopBuffer.begin(), stopBuffer.end());
                        file.seekg(-static_cast<int>(STOP_PATTERN.size() - 1), std::ios::cur);
                    }

                    if (!stopFound)
                    {
                        // Read remaining file if stop pattern not found
                        pixelData.insert(pixelData.end(), std::istreambuf_iterator<char>(file), {});
                    }

                    // Save the extracted DDS file
                    std::string outputFileName = "extracted_" + intToFilename(fileCount++) + ".dds";
                    std::ofstream outFile(outputFileName, std::ios::binary);
                    if (outFile.is_open()) 
                    {
                        outFile.write(reinterpret_cast<const char*>(fullHeader.data()), fullHeader.size());
                        outFile.write(reinterpret_cast<const char*>(pixelData.data()), pixelData.size());
                        outFile.close();
                        std::cout << "Saved DDS file: " << outputFileName << "\n";
                    }
                    else 
                    {
                        std::cerr << "Error: Could not save file: " << outputFileName << "\n";
                    }

                    // Reset buffer and continue search
                    break;
                }
            }
            file.seekg(-static_cast<int>(DDS_MAGIC.size() - 1), std::ios::cur);
        }

        file.close();
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

        fs::path output_file_path = file_path.parent_path() / ( hasher::CalculateHashOriginal( file_path.string().c_str() ) + ".dds" );

        std::ofstream outputFile(output_file_path, std::ios::binary);
        if (outputFile)
        {
            outputFile.write( reinterpret_cast<const char*>( dds_data.data() ), dds_data.size() );
            std::cout << "Extracted DDS data to: " << output_file_path << std::endl;
        }
    }

    void RenameNMHBinToHash(fs::path& file_path)
    {
        fs::path new_name = file_path.parent_path() / (hasher::CalculateHashOriginal(file_path.string().c_str()) + ".bin");
    
        fs::rename(file_path, new_name);
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

    void RemoveLast16BytesFromFile(const fs::path& nmh_bin_path)
    {
        std::fstream file(nmh_bin_path, std::ios::in | std::ios::out | std::ios::binary);

        file.seekg(0, std::ios::end);
        std::streampos file_size = file.tellg();

        std::streampos new_size = file_size - std::streamoff(16);

        file.seekg(0, std::ios::beg);
        std::vector<char> buffer(new_size);
        file.read(buffer.data(), new_size);
        file.close();

        std::ofstream out(nmh_bin_path, std::ios::out | std::ios::binary | std::ios::trunc);
        out.write(buffer.data(), buffer.size());

        std::cout << "Successfully removed the last 16 bytes from the file." << std::endl;
        out.close();
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
                fs::path file_path = entry.path();
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
                        case ExtractorMode::NMH_FIX_AND_HASH:
                        {
                            RemoveLast16BytesFromFile( file_path );
                            RenameNMHBinToHash( file_path );
                            break;
                        }
                        case ExtractorMode::BIG_TO_LITTLE_ENDIAN:
                        {
                            fs::path out = file_path.parent_path() / (file_path.stem().string() + "_le.bin");
                            convertBigEndianToLittleEndian(file_path, out);
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