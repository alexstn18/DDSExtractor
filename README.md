# DDSExtractor - .dds texture file extraction and re-importing tool for the PC port of killer7 (and probably No More Heroes too)

**Supported file extensions: .bin, .dat, .sti, .jmb**

In order to get access to such files, please use: https://github.com/Timo654/No-More-RSL

Please beware that this tool may not always work correctly, as the methods that I'm using for extraction and re-importing might not be the best ones. 
I am still working on improving this tool, and I am always open for feedback.

## Usage:
**Enter a path relative to the DDSExtractor.exe's location, e.g. "test_folder"**

**--extract**: Extracts textures in .dds format from various archives present in the path you've given the tool to work in.

**--import**: Re-imports textures (saved in the same folder by using `--extract`) into their original archives.

**--extracthashed**: Extracts textures in .dds format with MurmurHash variants for easy placement in the `Replacement` folder of Killer7.

**--nmhfixandhash**: for .bin GCT0 texture files from No More Heroes that are not hashed and have an extra 16 empty bytes at the end of the file.

**--bintodds**: for fixed and hashed .bin GCT0 texture files from No More Heroes, it converts them into DXT1 DDS image files.

## Requirements:
VCRedist: **https://aka.ms/vs/17/release/vc_redist.x64.exe**

## Additional credits:
- **SutandoTsukai181**: No More Hashes (https://web.archive.org/web/20230319040222/https://gist.github.com/SutandoTsukai181/dfe6884ee1254791ab166a0e876dda39)
- **Venomalia**: Dolphin Texture Extraction tool (https://github.com/Venomalia/DolphinTextureExtraction-tool)
