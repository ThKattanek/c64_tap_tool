#include <iostream>
#include <fstream>
#include <vector>
#include <math.h>

using namespace std;

#define VERSION_STRING "0.1"

#include "command_line_class.h"
#include <string.h>

// TAP Pulse Lengths (from VICE)
// Short Pulse between 288 and 432 Cycles
// Medium Pulse between 440 and 584 Cycles
// Long Pulse between 592 and 800 Cycles
// Cycles per second (PAL): 985248
// Cycles per second (NTSC): 1022727
#define SHORT_PULSE_MIN 288     // 0x24 (Databyte in TAP file)
#define SHORT_PULSE_MAX 432     // 0x36 (Databyte in TAP file)
#define MEDIUM_PULSE_MIN 440    // 0x37 (Databyte in TAP file)
#define MEDIUM_PULSE_MAX 584    // 0x49 (Databyte in TAP file)
#define LONG_PULSE_MIN 592      // 0x4A (Databyte in TAP file)
#define LONG_PULSE_MAX 800      // 0x64 (Databyte in TAP file)

// TAP Pulse Lengths for send to C64
// Cycles per second (PAL): 985248
// Cycles per second (NTSC): 1022727
#define SHORT_PULSE_LENGTH 360
#define MEDIUM_PULSE_LENGTH 524
#define LONG_PULSE_LENGTH 687

enum PULSE_TYPE {SHORT_PULSE, MEDIUM_PULSE, LONG_PULSE, UNKNOWN_PULSE};

typedef std::vector<uint8_t> ByteVector;
vector<ByteVector> current_block_list;

void AnalyzeTAPFile(const char *tap_file);
void ExportTAPFile(const char *tap_file);
bool ConvertPRGToTAP(const char *prg_file, const char *tap_file);
bool ConvertPRGToWAV(const char *prg_file_name, const char *wav_file_name);

// Defineren aller Kommandozeilen Parameter
enum CMD_COMMAND {CMD_HELP, CMD_VERSION, CMD_ANALYZE, CMD_EXPORT, CMD_CONVERT_TO_TAP, CMD_CONVERT_TO_WAV};
static const CMD_STRUCT command_list[]{
    {CMD_ANALYZE, "a", "analyze", "Analyzes the tap file. (c64_tap_tool --analyze <filename>)", 1},
    {CMD_EXPORT, "e", "export", "Export all files in this tap file as prg. (c64_tap_tool --export <filename>)", 1},
    {CMD_CONVERT_TO_TAP, "", "conv2tap", "Convert a prg to a tap file. (c64_tap_tool --conv2tap <prg_filename> <tap_filename>)", 2},
    {CMD_CONVERT_TO_WAV, "", "conv2wav", "Convert a prg to a wav file. (c64_tap_tool --conv2wav <prg_filename> <wav_filename>)", 2},
    {CMD_HELP, "?", "help", "This text.", 0},
    {CMD_VERSION, "", "version", "Displays the current version number.", 0}
};

#define command_list_count sizeof(command_list) / sizeof(command_list[0])

CommandLineClass *cmd;
uint8_t tap_version;

/// TAP Block Header
/// @brief  Kernal Header Block
/// @note   The Kernal Header Block is used to store the header information
///         of a TAP file. It contains the start and end address of the block,
///         the filename displayed on the C64, and the filename not displayed.
///         The header_typer is used to identify the type of block.
///         The filename displayed is limited to 16 characters, while the
///         filename not displayed can be up to 171 characters long.
///         The start and end address are 16-bit values, which means they can
///         represent addresses from 0x0000 to 0xFFFF.
struct KERNAL_HEADER_BLOCK    // C64 Kernal Header TAP Block
{
    uint8_t header_type;
    uint8_t start_address_low;
    uint8_t start_address_high;
    uint8_t end_address_low;
    uint8_t end_address_high;
    char filename_dispayed[16];
    char filename_not_displayed[171];
};

/// @brief  Main function of the program
/// @param argc      
/// @param argv 
/// @return 
int main(int argc, char *argv[]) 
{
    cmd = new CommandLineClass(argc, argv, "c64_tap_tool", command_list, command_list_count);

    if(cmd->GetCommandCount() <= 0)
    {
        printf("\"c64_tap_tool --help\" provides more information.\n");
        return(-1);
    }

    if(cmd->GetCommandCount() > 0)
    {
        for(int i=0; i<cmd->GetCommandCount(); i++)
        {
            if(cmd->GetCommand(i) == CMD_ANALYZE)
            {
                if(cmd->GetCommandCount() > i+1)
                {
                    const char *tap_file = cmd->GetArg(i+1);
                    printf("Analyzing TAP file: %s\n",tap_file);
                    AnalyzeTAPFile(tap_file);
                }
                else
                {
                    printf("Missing TAP file.\n");
                    return(-1);
                }
            }

            if(cmd->GetCommand(i) == CMD_EXPORT)
            {
                if(cmd->GetCommandCount() > i+1)
                {
                    const char *tap_file = cmd->GetArg(i+1);
                    printf("Export all files in this TAP file as PRG: %s\n",tap_file);
                    ExportTAPFile(tap_file);
                }
                else
                {
                    printf("Missing TAP file.\n");
                    return(-1);
                }
            }

            if(cmd->GetCommand(i) == CMD_CONVERT_TO_TAP)
            {
                printf("Convert PRG to TAP file.\n");
                ConvertPRGToTAP(cmd->GetArg(i+1), cmd->GetArg(i+2));
            }

            if(cmd->GetCommand(i) == CMD_CONVERT_TO_WAV)
            {
                printf("Convert PRG to WAV file.\n");
                ConvertPRGToWAV(cmd->GetArg(i+1), cmd->GetArg(i+2));
            }

        }

        if(cmd->FoundCommand(CMD_HELP))
        {
            cmd->ShowHelp();
            return 0;
        }

        if(cmd->GetCommand(0) == CMD_VERSION)
        {
            printf("C64 TAP Tool - Version: %s\n\n",VERSION_STRING);
            return(0x0);
        }
    }

    return 0;
}

/// @brief  Check if the given data is a valid TAP file
/// @param data  Pointer to the TAP file data
/// @param size  Size of the TAP file data
/// @return  True if the data is a valid TAP file, false otherwise
/// @note   The TAP file must start with the string "C64-TAPE-RAW"
///         and the version number must be present.
bool IsTAPFile(uint8_t *data, uint32_t size)
{
    // Prüfe ob am Anfang der Datei C64-TAPE_RAW steht
    const char header[] = "C64-TAPE-RAW";
    if(memcmp(data, header, sizeof(header)-1) != 0)
    {
        return false;
    }

    // TAP Version
    tap_version = data[sizeof(header)-1];

    return true;
}

/// @brief  Get the next pulse from the TAP file
/// @param data  Pointer to the TAP file data
/// @param pos  Current position in the TAP file data
/// @return  Type of the pulse (Short, Medium, Long, Unknown)
uint8_t GetNextPulse(uint8_t *data, uint32_t &pos)
{
    uint32_t pulse_length = data[pos];
    uint8_t pulse_type;
    
    if(pulse_length == 0x00)
    {
        if(tap_version == 0)
        {
            pulse_length = 256 * 8;
        }
            
        if(tap_version == 1)
        {
            pulse_length = data[pos+1] | data[pos+2] << 8 | data[pos+3] << 16;
            pos += 3;
        }
    }
    else
    {
        pulse_length *= 8;
    }

    if(pulse_length >= SHORT_PULSE_MIN && pulse_length <= SHORT_PULSE_MAX)
        {
            pulse_type = SHORT_PULSE;
        }
        else if(pulse_length >= MEDIUM_PULSE_MIN && pulse_length <= MEDIUM_PULSE_MAX)
        {
            pulse_type = MEDIUM_PULSE;
        }
        else if(pulse_length >= LONG_PULSE_MIN && pulse_length <= LONG_PULSE_MAX)
        {
            pulse_type = LONG_PULSE;
        }
        else
        {
            pulse_type = UNKNOWN_PULSE;
        }

    return pulse_type;
}

/// @brief  Get the next byte from the TAP file
/// @param data  Pointer to the TAP file data
/// @param size  Size of the TAP file data
/// @param pos  Current position in the TAP file data
/// @param error  Error flag
/// @return  Next byte from the TAP file
uint8_t GetNextKernalByte(uint8_t *data, uint32_t size, uint32_t &pos, bool &error, bool &start_new_block)
{
    u_int32_t sync_start = 0;
    u_int32_t sync_end = 0;
    uint32_t sync_pulse_count = 0;
    bool found_sync = false;

    uint8_t last_pulse = 0;  // 0 = Short, 1 = Medium, 2 = Long
    uint8_t pulse_counter = 0;
    bool byte_reading = false;
    uint8_t parity_bit = 1;
    uint8_t data_byte = 0;

    start_new_block = false;
    error = false;

    while (pos < size)
    {
        uint8_t pulse_type = GetNextPulse(data, pos);

        switch (pulse_type)
        {
        case PULSE_TYPE::SHORT_PULSE:
            // Short Pulse
            pulse_counter++;
            sync_pulse_count++;

            if((sync_pulse_count > 1) && !found_sync)
            {
                sync_start = pos-1;
                found_sync = true;
            }

            if(byte_reading)
            {
                if(((pulse_counter & 1) == 0) && (last_pulse == MEDIUM_PULSE))
                {
                    // Bit is 1
                    if(pulse_counter <= 16)
                    {
                        data_byte >>= 1;
                        data_byte |= 0x80;
                        parity_bit ^= 1;
                    }
                    else if(pulse_counter == 18)
                    {
                        // Parity Check
                        if(parity_bit == 0)
                        {
                            error = true;
                        }
                        else error = false;
                        return data_byte;
                    }
                }
            }

            last_pulse = SHORT_PULSE;
            break;
        
        case PULSE_TYPE::MEDIUM_PULSE:
            // Medium Pulse
            pulse_counter++;

            if(found_sync)
            {
                sync_end = pos-1;
                found_sync = false;
                if(sync_end - sync_start >= 2) 
                {
                    start_new_block = true;
                    printf("Sync found: %4.4x - %4.4x (%d pulses)\n",sync_start,sync_end, sync_end - sync_start);
                }
            }

            if(byte_reading)
            {
                if(((pulse_counter & 1) == 0) && (last_pulse == SHORT_PULSE))
                {
                    // Bit is 0
                    if(pulse_counter <= 16)
                    {
                        data_byte >>= 1;
                        data_byte &= 0x7f;
                        parity_bit ^= 0;
                    }
                    else if(pulse_counter == 18)
                    {
                        // Parity Check
                        if(parity_bit == 1)
                        {
                            printf("Parity Error: %4.4x - %4.4x (%d pulses)\n",sync_start,sync_end, sync_end - sync_start);
                            error = true;
                        }
                        else error = false;
                        return data_byte;
                   }
                }
            }
            
            // Check if last pulse was a short pulse the is here a ByteMarker
            if(last_pulse == LONG_PULSE)
            {
                byte_reading = true;
                pulse_counter = 0;
            }

            sync_pulse_count = 0;
            last_pulse = MEDIUM_PULSE;
            break;
        case PULSE_TYPE::LONG_PULSE:
            // Long Pulse
            pulse_counter++;

            if(found_sync)
            {
                sync_end = pos-1;
                found_sync = false;
                if(sync_end - sync_start >= 2)
                {
                    start_new_block = true;
                    printf("Sync found: %4.4x - %4.4x (%d pulses)\n",sync_start,sync_end, sync_end - sync_start);
                }
            }
            sync_pulse_count = 0;
            last_pulse = LONG_PULSE;   
            break;

        case PULSE_TYPE::UNKNOWN_PULSE:
            /* code */
                //printf("Unknown Pulse at: %4.4x\n", pos);
            break;

        default:
            break;
        }
        pos++;
    }
    error = true;
    return 0;
}

/// @brief  Find all kernal blocks in the TAP file
/// @param data  Pointer to the TAP file data
/// @param size  Size of the TAP file data
/// @param block_list  List of kernal blocks
/// @return  True if all blocks are found, false otherwise
bool FindAllKernalBlocks(uint8_t *data, uint32_t size, vector<ByteVector> &block_list)
{
    uint32_t pos = 0X14;    // start data's at position 0x14 in TAP file 
    bool error;
    bool start_new_block;
    bool ret = true;

    block_list.clear();
    ByteVector *current_block;

    while(pos < size)
    {
        uint8_t data_byte = GetNextKernalByte(data, size, pos, error, start_new_block);
        if(!error)
        {
            if(start_new_block)
            {
                // Start new block and add first byte to it
                block_list.push_back(ByteVector());
                current_block = &block_list.back();
                current_block->push_back(data_byte);
            }
            else
            {
                // Add byte to current block
                current_block->push_back(data_byte);
            }
        }
        else
        {
            if(pos < size)
            {
                ret = false;
                printf("Error reading byte at position %4.4x\n",pos);
            }
            else
            {
                printf("End of TAP file reached.\n");
            }
        }  
    }

    printf("Block Count: %ld\n", block_list.size());

    // CRC Checking
    for(int i=0; i < (int)block_list.size(); i++)
    {
        printf("Block %d Size: %ld [CRC: ", i, block_list[i].size());
        uint8_t crc = 0;
        for(int j=9; j < (int)block_list[i].size()-1; j++)
        {
            crc ^= block_list[i][j];
        }
        if(crc == block_list[i].back())
        {
            printf("OK]");
        }
        else
        {
            ret = false;
            printf("Error]");
        }
    

        uint8_t countdown;
        
        if((i & 1) == 1)
            countdown = 0x09; 
        else
            countdown = 0x89;

        bool countdown_io = true;
        for(int j=0; j<9; j++)
        {
            if(block_list[i][j] != countdown)
                countdown_io = false;
            countdown--;
        }

        printf(" - [Countdown: ");
        
        if(countdown_io)
            printf("OK]\n");
        else
        {
            ret = false;
            printf("Error]\n");
        }
    }

    return ret;
}

/// @brief  Analyze the TAP file and find all kernal blocks
/// @param tap_file  Path to the TAP file
/// @note   The TAP file must be in binary format
///         and must start with the string "C64-TAPE-RAW".
///         The version number must be present.
///         The function will read the TAP file and find all kernal blocks.
void AnalyzeTAPFile(const char *tap_file)
{
    std::ifstream tap_file_stream(tap_file, ios::binary);
    if(tap_file_stream.is_open())
    {
        tap_file_stream.seekg(0, ios::end);
        streamoff file_size = tap_file_stream.tellg();
        tap_file_stream.seekg(0, ios::beg);

        uint8_t *tap_data = new uint8_t[file_size];
        tap_file_stream.read((char*)tap_data, file_size);
        tap_file_stream.close();

        printf("TAP file size: %ld\n", static_cast<long>(file_size));
        
        if(IsTAPFile(tap_data, (uint32_t)file_size))
        {
            printf("TAP file is valid.\n");
            printf("TAP version: %d\n",tap_version);
            if(FindAllKernalBlocks(tap_data, (uint32_t)file_size, current_block_list))
            {
                for(int i=0; i < (int)current_block_list.size(); i++)
                {
                    KERNAL_HEADER_BLOCK *kernal_header_block = (KERNAL_HEADER_BLOCK *)&current_block_list[i][9];
                    if(current_block_list[i].size() == 202 && (kernal_header_block->header_type >= 0x01) && (kernal_header_block->header_type <= 0x05))
                    {
                        printf("Block %d: Kernal Header Block", i);
                        if((current_block_list[i][0] & 0x80) != 0x80)
                        {
                            printf(" [BACKUP]\n");
                        }
                        else
                        {
                            printf("\n");
                        }
                        printf("Start Address: %4.4x\n", kernal_header_block->start_address_low | (kernal_header_block->start_address_high << 8));
                        printf("End Address: %4.4x\n", kernal_header_block->end_address_low | (kernal_header_block->end_address_high << 8));
                        for(int j=15; j>0; j--)
                        {
                            if(kernal_header_block->filename_dispayed[j] == 0x20)
                            {
                                kernal_header_block->filename_dispayed[j] = 0;
                            }
                            else
                            {
                                break;
                            }
                        }
                        kernal_header_block->filename_dispayed[15] = 0;
                        printf("Filename: %s\n", kernal_header_block->filename_dispayed);
                        printf("Filename displayed: %s\n", kernal_header_block->filename_dispayed);
                    }
                }
            }
            else
            {
                printf("Error finding kernal blocks.\n");
            }
        }
        else
        {
            printf("TAP file is invalid.\n");
        }

        delete[] tap_data;
    }
    else
    {
        printf("Error opening TAP file: %s\n",tap_file);
    }
}

void ExportTAPFile(const char *tap_file)
{
    AnalyzeTAPFile(tap_file);

    for(int i=0; i < (int)current_block_list.size(); i++)
    {
        KERNAL_HEADER_BLOCK *kernal_header_block = (KERNAL_HEADER_BLOCK *)&current_block_list[i][9];
        if(current_block_list[i].size() == 202 && (kernal_header_block->header_type >= 0x01) && ((current_block_list[i][0] & 0x80) == 0x80))
        {
            kernal_header_block->filename_dispayed[15] = 0;
            printf("Exporting Block %d: %s\n", i, kernal_header_block->filename_dispayed);
            std::ofstream prg_file(kernal_header_block->filename_dispayed + std::string(".prg"), ios::binary);
            if(prg_file.is_open())
            {
                prg_file.write((const char*)&kernal_header_block->start_address_low, 1);
                prg_file.write((const char*)&kernal_header_block->start_address_high, 1);
                prg_file.write((char*)&current_block_list[i+2][9], current_block_list[i+2].size()-9);
                prg_file.close();
            }
            else
            {
                printf("Error opening PRG file: %s\n",kernal_header_block->filename_dispayed);
            }
        }
    }
}

inline uint32_t WriteTAPShortPulse(std::ofstream &tap_stream, uint32_t pulse_count) 
{
    uint8_t short_pulse_len = SHORT_PULSE_LENGTH >> 3;
    for (uint32_t pulse = 0; pulse < pulse_count; ++pulse) 
    {
        tap_stream.write(reinterpret_cast<const char*>(&short_pulse_len), 1);
    }
    return pulse_count;
}

inline uint32_t WriteTAPMediumPulse(std::ofstream &tap_stream, uint32_t pulse_count) 
{
    uint8_t medium_pulse_len = MEDIUM_PULSE_LENGTH >> 3;
    for (uint32_t pulse = 0; pulse < pulse_count; ++pulse) 
    {
        tap_stream.write(reinterpret_cast<const char*>(&medium_pulse_len), 1);
    }
    return pulse_count;
}

inline uint32_t WriteTAPLongPulse(std::ofstream &tap_stream, uint32_t pulse_count) 
{
    uint8_t long_pulse_len = LONG_PULSE_LENGTH >> 3;
    for (uint32_t pulse = 0; pulse < pulse_count; ++pulse) 
    {
        tap_stream.write(reinterpret_cast<const char*>(&long_pulse_len), 1);
    }
    return pulse_count;
}

inline uint32_t WriteTAPByte(std::ofstream &tap_stream, uint8_t byte) 
{
    uint32_t num_samples = 0;

    // Write the byte in the TAP file
    // ByteMaker (Long Pulse + Medium Pulse)
    num_samples += WriteTAPLongPulse(tap_stream, 1);
    num_samples += WriteTAPMediumPulse(tap_stream, 1);

    // Write the bits of the byte (LSB first)
    uint8_t parity_bit = 1;
    for (int i = 0; i < 8; ++i)     
    {
        if (byte & (1 << i)) 
        {
            // Bit is 1
            num_samples += WriteTAPMediumPulse(tap_stream, 1);
            num_samples += WriteTAPShortPulse(tap_stream, 1);
            parity_bit ^= 1;
        } else 
        {
            // Bit is 0
            num_samples += WriteTAPShortPulse(tap_stream, 1);
            num_samples += WriteTAPMediumPulse(tap_stream, 1);
        }
    }
    // Write the parity bit (odd parity)
    if (parity_bit == 1) 
    {
        num_samples += WriteTAPMediumPulse(tap_stream, 1);
        num_samples += WriteTAPShortPulse(tap_stream, 1);
    } else 
    {
        num_samples += WriteTAPShortPulse(tap_stream, 1);
        num_samples += WriteTAPMediumPulse(tap_stream, 1);
    }

    return num_samples;
}

bool ConvertPRGToTAP(const char *prg_file_name, const char *tap_file_name)
{
    // TODO: Implement the conversion from PRG to TAP
    // TAP write 
    // C64 PAL Frquency: 985248 Hz
    // ● a short 365.4µs pulse (2737 Hz) PAL - 360 Takte    
    // ● a medium 531.4µs pulse (1882Hz) PAL - 524 Takte
    // ● a long 697.6µs pulse (1434 Hz) PAL - 687 Takte

    // 1. Short pulse (27135)
    // 2. Countdown Sequence 0x89 0x88 0x87 0x86 0x85 0x84 0x83 0x82 0x81
    // 3. Kernal Header Block
    // 3a. EndOfData Maker
    // 4. Short pulse (79)
    // 5. Countdown Sequence 0x09 0x08 0x07 0x06 0x05 0x04 0x03 0x02 0x01
    // 6. Kernal Header Block (Backup)
    // 7. Short pulse (5671)
    // 9. Countdown Sequence 0x89 0x88 0x87 0x86 0x85 0x84 0x83 0x82 0x81
    // 10. Kernal Data Block
    // 10a. EndOfData Maker
    // 11. Short pulse (79)
    // 12. Countdown Sequence 0x09 0x08 0x07 0x06 0x05 0x04 0x03 0x02 0x01
    // 13. Kernal Data Block (Backup)

    ifstream prg_stream;
    ofstream tap_stream;
    prg_stream.open(prg_file_name, ios::binary);
    if(!prg_stream.is_open())
    {
        printf("Error opening PRG file: %s\n", prg_file_name);
        return false;
    }

    // PRG file size    
    prg_stream.seekg(0, ios::end);
    streamoff prg_file_size = prg_stream.tellg();
    prg_stream.seekg(0, ios::beg);

    tap_stream.open(tap_file_name, ios::binary);
    if(!tap_stream.is_open())
    {
        printf("Error opening TAP file: %s\n", tap_file_name);
        prg_stream.close();
        return false;
    }

    // TAP Header
    tap_stream.write("C64-TAPE-RAW", 12); // TAP Header
    tap_stream.write(reinterpret_cast<const char*>(&tap_version), 1); // TAP Version
    uint8_t tap_header[3] = {0x00, 0x00, 0x00}; // TAP Header (Future expanison)
    tap_stream.write(reinterpret_cast<const char*>(tap_header), sizeof(tap_header));
    uint32_t tap_data_size = 0; // TAP Data Size
    tap_stream.write(reinterpret_cast<const char*>(&tap_data_size), 4); // TAP Data Size

    // Create the WAV File for the C64
    // Start with 27135 short pulses (10sec Syncronisation)
    tap_data_size += WriteTAPShortPulse(tap_stream, 27135);

    // Countdown Sequence (none backup)
    for (uint8_t countdown = 0x89; countdown >= 0x81; countdown--)
    {
        tap_data_size += WriteTAPByte(tap_stream, countdown);
    }

    // Kernal Header Block
    KERNAL_HEADER_BLOCK kernal_header_block;
    prg_stream.read(reinterpret_cast<char*>(&kernal_header_block.start_address_low), 1);
    prg_stream.read(reinterpret_cast<char*>(&kernal_header_block.start_address_high), 1);
    prg_file_size -= 2;

    uint32_t temp_address = (kernal_header_block.start_address_low | (kernal_header_block.start_address_high << 8));
        
    uint16_t end_adress = static_cast<uint16_t>(temp_address);
    end_adress += static_cast<uint16_t>(prg_file_size);
    kernal_header_block.end_address_low = static_cast<uint8_t>(end_adress & 0x00FF);
    kernal_header_block.end_address_high = static_cast<uint8_t>((end_adress >> 8) & 0x00FF);

    kernal_header_block.header_type = 0x01; // Kernal Header Block
    memset(kernal_header_block.filename_dispayed, 0x20, sizeof(kernal_header_block.filename_dispayed));
    memset(kernal_header_block.filename_not_displayed, 0x20, sizeof(kernal_header_block.filename_not_displayed));

    const char *filename_displayed = "C64-TAP-TOOL";
    const char *filename_not_displayed = "";

    strncpy(kernal_header_block.filename_dispayed, filename_displayed, strlen(filename_displayed));
    strncpy(kernal_header_block.filename_not_displayed, filename_not_displayed, strlen(filename_not_displayed));
   
    // Write the Kernal Header Block to the WAV file
    uint8_t crc = 0;
    for(int i=0; i < (int)sizeof(kernal_header_block); i++)
    {
        crc ^= ((uint8_t*)&kernal_header_block)[i];
        tap_data_size += WriteTAPByte(tap_stream, ((uint8_t*)&kernal_header_block)[i]);
    }
    tap_data_size += WriteTAPByte(tap_stream, crc);

    // Write the EndOfData Maker
    tap_data_size += WriteTAPLongPulse(tap_stream, 1); 
    tap_data_size += WriteTAPShortPulse(tap_stream, 1);

    // Start with 79 short pulses
    tap_data_size += WriteTAPShortPulse(tap_stream, 79);

    // Countdown Sequence (backup)
    for (uint8_t countdown = 0x09; countdown >= 0x01; countdown--)
    {
        tap_data_size += WriteTAPByte(tap_stream, countdown);
    }

    // Kernal Header Block (Backup)
    crc = 0;
    for(int i=0; i < (int)sizeof(kernal_header_block); i++)
    {
        crc ^= ((uint8_t*)&kernal_header_block)[i];
        tap_data_size += WriteTAPByte(tap_stream, ((uint8_t*)&kernal_header_block)[i]);
    }
    tap_data_size += WriteTAPByte(tap_stream, crc);

    // Start with 5671 short pulses (2sec Syncronisation)
    tap_data_size += WriteTAPShortPulse(tap_stream, 5671);

    // Countdown Sequence (none backup)
    for (uint8_t countdown = 0x89; countdown >= 0x81; countdown--)
    {
        tap_data_size += WriteTAPByte(tap_stream, countdown);
    }

    // Kernal Data Block
    // Write the PRG file data to the WAV file
    crc = 0;
    uint8_t byte;
    for(int i=0; i < (int)prg_file_size; i++)
    {
        prg_stream.read(reinterpret_cast<char*>(&byte), 1);
        crc ^= byte;
        tap_data_size += WriteTAPByte(tap_stream, byte);
    }
    tap_data_size += WriteTAPByte(tap_stream, crc);
    
    // Write the EndOfData Maker        
    tap_data_size += WriteTAPLongPulse(tap_stream, 1);
    tap_data_size += WriteTAPShortPulse(tap_stream, 1);

    // Start with 79 short pulses (10sec Syncronisation)
    tap_data_size += WriteTAPShortPulse(tap_stream, 79);    

    // Countdown Sequence (backup)
    for (uint8_t countdown = 0x09; countdown >= 0x01; countdown--)
    {
        tap_data_size += WriteTAPByte(tap_stream, countdown);
    }

    // Kernal Data Block (Backup)
    prg_stream.seekg(2, ios::beg);
    crc = 0;
    for(int i=0; i < (int)prg_file_size; i++)
    {
        prg_stream.read(reinterpret_cast<char*>(&byte), 1);
        crc ^= byte;
        tap_data_size += WriteTAPByte(tap_stream, byte);
    }
    tap_data_size += WriteTAPByte(tap_stream, crc);

    // Write the EndOfData Maker (Optional)
    //tap_data_size += WriteTAPLongPulse(tap_stream, 1); 
    //tap_data_size += WriteTAPShortPulse(tap_stream, 1);

    // Update the TAP Data Size
    tap_stream.seekp(16, ios::beg);
    tap_stream.write(reinterpret_cast<const char*>(&tap_data_size), 4); // TAP Data Size

    prg_stream.close();
    tap_stream.close();

    return true;
}

// Funktion zum Erstellen des WAV-Headers
void WriteWAVHeader(std::ofstream &wav_file, uint32_t sample_rate, uint32_t num_samples) {
    uint32_t byte_rate = sample_rate * sizeof(float); // Mono, Float
    uint16_t block_align = sizeof(float);            // Mono, Float
    uint32_t data_chunk_size = num_samples * sizeof(float);
    uint32_t file_size = 36 + data_chunk_size;

    // WAV-Header schreiben
    wav_file.write("RIFF", 4);                        // Chunk ID
    wav_file.write(reinterpret_cast<const char*>(&file_size), 4); // Chunk Size
    wav_file.write("WAVE", 4);                        // Format
    wav_file.write("fmt ", 4);                        // Subchunk1 ID
    uint32_t subchunk1_size = 16;                     // Subchunk1 Size
    wav_file.write(reinterpret_cast<const char*>(&subchunk1_size), 4);
    uint16_t audio_format = 3;                        // Audio Format (3 = Float)
    wav_file.write(reinterpret_cast<const char*>(&audio_format), 2);
    uint16_t num_channels = 1;                        // Mono
    wav_file.write(reinterpret_cast<const char*>(&num_channels), 2);
    wav_file.write(reinterpret_cast<const char*>(&sample_rate), 4); // Sample Rate
    wav_file.write(reinterpret_cast<const char*>(&byte_rate), 4);   // Byte Rate
    wav_file.write(reinterpret_cast<const char*>(&block_align), 2); // Block Align
    uint16_t bits_per_sample = 32;                    // Float = 32 bits
    wav_file.write(reinterpret_cast<const char*>(&bits_per_sample), 2);
    wav_file.write("data", 4);                        // Subchunk2 ID
    wav_file.write(reinterpret_cast<const char*>(&data_chunk_size), 4); // Subchunk2 Size
}

inline uint32_t WriteWAVShortPulse(std::ofstream &wav_stream, uint32_t sample_rate, uint32_t pulse_count, float amplitude = 1.0f) 
{
    const float frequency = 2737.0f; // Frequenz des Shortpulses in Hz
    const uint32_t samples_per_period = static_cast<uint32_t>(static_cast<float>(sample_rate) / frequency);

    for (uint32_t pulse = 0; pulse < pulse_count; ++pulse) {
        for (uint32_t sample = 0; sample < samples_per_period; ++sample) {
            float t = static_cast<float>(sample) / static_cast<float>(sample_rate); // Zeit in Sekunden
            float value = amplitude * sinf(2.0f * static_cast<float>(M_PI) * frequency * t) *-1.0f; // Invert the signal for short pulse
            wav_stream.write(reinterpret_cast<const char*>(&value), sizeof(float));
        }
    }

    return pulse_count * samples_per_period;
}

inline uint32_t WriteWAVMediumPulse(std::ofstream &wav_stream, uint32_t sample_rate, uint32_t pulse_count, float amplitude = 1.0f) 
{
    const float frequency = 1882.0f; // Frequenz des Mediumpulses in Hz
    const uint32_t samples_per_period = static_cast<uint32_t>(static_cast<float>(sample_rate) / frequency);

    for (uint32_t pulse = 0; pulse < pulse_count; ++pulse) {
        for (uint32_t sample = 0; sample < samples_per_period; ++sample) {
            float t = static_cast<float>(sample) / static_cast<float>(sample_rate); // Zeit in Sekunden
            float value = amplitude * sinf(2.0f * static_cast<float>(M_PI) * frequency * t) *-1.0f; // Invert the signal for medium pulse
            wav_stream.write(reinterpret_cast<const char*>(&value), sizeof(float));
        }
    }

    return pulse_count * samples_per_period;
}

inline uint32_t WriteWAVLongPulse(std::ofstream &wav_stream, uint32_t sample_rate, uint32_t pulse_count, float amplitude = 1.0f) 
{
    const float frequency = 1434.0f; // Frequenz des Longpulses in Hz
    const uint32_t samples_per_period = static_cast<uint32_t>(static_cast<float>(sample_rate) / frequency);

    for (uint32_t pulse = 0; pulse < pulse_count; ++pulse) {
        for (uint32_t sample = 0; sample < samples_per_period; ++sample) {
            float t = static_cast<float>(sample) / static_cast<float>(sample_rate); // Zeit in Sekunden
            float value = amplitude * sinf(2.0f * static_cast<float>(M_PI) * frequency * t) *-1.0f; // Invert the signal for long pulse
            wav_stream.write(reinterpret_cast<const char*>(&value), sizeof(float));
        }
    }

    return pulse_count * samples_per_period;
}

inline uint32_t WriteWAVByte(std::ofstream &wav_stream, uint32_t sample_rate, uint8_t byte, float amplitude = 1.0f) 
{
    uint32_t num_samples = 0;

    // Write the byte in the WAV file
    // ByteMaker (Long Pulse + Medium Pulse)
    num_samples += WriteWAVLongPulse(wav_stream, sample_rate, 1, amplitude);
    num_samples += WriteWAVMediumPulse(wav_stream, sample_rate, 1, amplitude);

    // Write the bits of the byte (LSB first)
    uint8_t parity_bit = 1;
    for (int i = 0; i < 8; ++i)     
    {
        if (byte & (1 << i)) 
        {
            // Bit is 1
            num_samples += WriteWAVMediumPulse(wav_stream, sample_rate, 1, amplitude);
            num_samples += WriteWAVShortPulse(wav_stream, sample_rate, 1, amplitude);
            parity_bit ^= 1;
        } else 
        {
            // Bit is 0
            num_samples += WriteWAVShortPulse(wav_stream, sample_rate, 1, amplitude);
            num_samples += WriteWAVMediumPulse(wav_stream, sample_rate, 1, amplitude);
        }
    }
    // Write the parity bit (odd parity)
    if (parity_bit == 1) 
    {
        num_samples += WriteWAVMediumPulse(wav_stream, sample_rate, 1, amplitude);
        num_samples += WriteWAVShortPulse(wav_stream, sample_rate, 1, amplitude);
    } else 
    {
        num_samples += WriteWAVShortPulse(wav_stream, sample_rate, 1, amplitude);
        num_samples += WriteWAVMediumPulse(wav_stream, sample_rate, 1, amplitude);
    }

    return num_samples;
}

bool ConvertPRGToWAV(const char *prg_file_name, const char *wav_file_name)
{
    // TODO: Implement the conversion from PRG to TAP
    // TAP write 
    // C64 PAL Frquency: 985248 Hz
    // ● a short 365.4µs pulse (2737 Hz) PAL - 360 Takte    
    // ● a medium 531.4µs pulse (1882Hz) PAL - 524 Takte
    // ● a long 697.6µs pulse (1434 Hz) PAL - 687 Takte

    // 1. Short pulse (27135)
    // 2. Countdown Sequence 0x89 0x88 0x87 0x86 0x85 0x84 0x83 0x82 0x81
    // 3. Kernal Header Block
    // 3a. EndOfData Maker
    // 4. Short pulse (79)
    // 5. Countdown Sequence 0x09 0x08 0x07 0x06 0x05 0x04 0x03 0x02 0x01
    // 6. Kernal Header Block (Backup)
    // 7. Short pulse (5671)
    // 9. Countdown Sequence 0x89 0x88 0x87 0x86 0x85 0x84 0x83 0x82 0x81
    // 10. Kernal Data Block
    // 10a. EndOfData Maker
    // 11. Short pulse (79)
    // 12. Countdown Sequence 0x09 0x08 0x07 0x06 0x05 0x04 0x03 0x02 0x01
    // 13. Kernal Data Block (Backup)

    ifstream prg_stream;
    ofstream wav_stream;
    prg_stream.open(prg_file_name, ios::binary);
    if(!prg_stream.is_open())
    {
        printf("Error opening PRG file: %s\n", prg_file_name);
        return false;
    }

    // PRG file size    
    prg_stream.seekg(0, ios::end);
    streamoff prg_file_size = prg_stream.tellg();
    prg_stream.seekg(0, ios::beg);

    wav_stream.open(wav_file_name, ios::binary);
    if(!wav_stream.is_open())
    {
        printf("Error opening WAV file: %s\n", wav_file_name);
        prg_stream.close();
        return false;
    }

    // WAV Header
    uint32_t sample_rate = 44100; // Sample rate in Hz
    uint32_t num_samples = 0; // Number of samples in the WAV file
    WriteWAVHeader(wav_stream, sample_rate, num_samples);

    // Create the WAV File for the C64
    // Start with 27135 short pulses (10sec Syncronisation)
    num_samples += WriteWAVShortPulse(wav_stream, sample_rate, 27135);

    // Countdown Sequence (none backup)
    for (uint8_t countdown = 0x89; countdown >= 0x81; countdown--)
    {
        num_samples += WriteWAVByte(wav_stream, sample_rate, countdown);
    }

    // Kernal Header Block
    KERNAL_HEADER_BLOCK kernal_header_block;
    prg_stream.read(reinterpret_cast<char*>(&kernal_header_block.start_address_low), 1);
    prg_stream.read(reinterpret_cast<char*>(&kernal_header_block.start_address_high), 1);
    prg_file_size -= 2;

    uint32_t temp_address = (kernal_header_block.start_address_low | (kernal_header_block.start_address_high << 8));
        
    uint16_t end_adress = static_cast<uint16_t>(temp_address);
    end_adress += static_cast<uint16_t>(prg_file_size);
    kernal_header_block.end_address_low = static_cast<uint8_t>(end_adress & 0x00FF);
    kernal_header_block.end_address_high = static_cast<uint8_t>((end_adress >> 8) & 0x00FF);

    kernal_header_block.header_type = 0x01; // Kernal Header Block
    memset(kernal_header_block.filename_dispayed, 0x20, sizeof(kernal_header_block.filename_dispayed));
    memset(kernal_header_block.filename_not_displayed, 0x20, sizeof(kernal_header_block.filename_not_displayed));

    const char *filename_displayed = "C64-TAP-TOOL";
    const char *filename_not_displayed = "";

    strncpy(kernal_header_block.filename_dispayed, filename_displayed, strlen(filename_displayed));
    strncpy(kernal_header_block.filename_not_displayed, filename_not_displayed, strlen(filename_not_displayed));
   
    // Write the Kernal Header Block to the WAV file
    uint8_t crc = 0;
    for(int i=0; i < (int)sizeof(kernal_header_block); i++)
    {
        crc ^= ((uint8_t*)&kernal_header_block)[i];
        num_samples += WriteWAVByte(wav_stream, sample_rate, ((uint8_t*)&kernal_header_block)[i]);
    }
    num_samples += WriteWAVByte(wav_stream, sample_rate, crc);

    // Write the EndOfData Maker
    num_samples += WriteWAVLongPulse(wav_stream, sample_rate, 1); 
    num_samples += WriteWAVShortPulse(wav_stream, sample_rate, 1);

    // Start with 79 short pulses (10sec Syncronisation)
    num_samples += WriteWAVShortPulse(wav_stream, sample_rate, 79);


    // Countdown Sequence (backup)
    for (uint8_t countdown = 0x09; countdown >= 0x01; countdown--)
    {
        num_samples += WriteWAVByte(wav_stream, sample_rate, countdown);
    }

    // Kernal Header Block (Backup)
    crc = 0;
    for(int i=0; i < (int)sizeof(kernal_header_block); i++)
    {
        crc ^= ((uint8_t*)&kernal_header_block)[i];
        num_samples += WriteWAVByte(wav_stream, sample_rate, ((uint8_t*)&kernal_header_block)[i]);
    }
    num_samples += WriteWAVByte(wav_stream, sample_rate, crc);

    // Start with 5671 short pulses (10sec Syncronisation)
    num_samples += WriteWAVShortPulse(wav_stream, sample_rate, 5671);

    // Countdown Sequence (none backup)
    for (uint8_t countdown = 0x89; countdown >= 0x81; countdown--)
    {
        num_samples += WriteWAVByte(wav_stream, sample_rate, countdown);
    }

    // Kernal Data Block
    // Write the PRG file data to the WAV file
    crc = 0;
    uint8_t byte;
    for(int i=0; i < (int)prg_file_size; i++)
    {
        prg_stream.read(reinterpret_cast<char*>(&byte), 1);
        crc ^= byte;
        num_samples += WriteWAVByte(wav_stream, sample_rate, byte);
    }
    num_samples += WriteWAVByte(wav_stream, sample_rate, crc);
    
    // Write the EndOfData Maker        
    num_samples += WriteWAVLongPulse(wav_stream, sample_rate, 1);
    num_samples += WriteWAVShortPulse(wav_stream, sample_rate, 1);

    // Start with 79 short pulses (10sec Syncronisation)
    num_samples += WriteWAVShortPulse(wav_stream, sample_rate, 79);    

    // Countdown Sequence (backup)
    for (uint8_t countdown = 0x09; countdown >= 0x01; countdown--)
    {
        num_samples += WriteWAVByte(wav_stream, sample_rate, countdown);
    }

    // Kernal Data Block (Backup)
    prg_stream.seekg(2, ios::beg);
    crc = 0;
    for(int i=0; i < (int)prg_file_size; i++)
    {
        prg_stream.read(reinterpret_cast<char*>(&byte), 1);
        crc ^= byte;
        num_samples += WriteWAVByte(wav_stream, sample_rate, byte);
    }
    num_samples += WriteWAVByte(wav_stream, sample_rate, crc);

    // Write the EndOfData Maker (Optional)
    //num_samples += WriteTAPLongPulse(tap_stream, 1); 
    //num_samples += WriteTAPShortPulse(tap_stream, 1);

    // Update the WAV header with the correct data chunk size
    wav_stream.seekp(0, ios::beg);
    WriteWAVHeader(wav_stream, sample_rate, num_samples);

    prg_stream.close();
    wav_stream.close();
    
    return true;
}