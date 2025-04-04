#include <iostream>
#include <fstream>
#include <vector>

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

void AnalyzeTAPFile(const char *tap_file);

// Defineren aller Kommandozeilen Parameter
enum CMD_COMMAND {CMD_HELP,CMD_VERSION, CMD_ANALYZE};
static const CMD_STRUCT command_list[]{
    {CMD_ANALYZE, "a", "analyze", "Analyzes the TAP file.", 1},
    {CMD_HELP, "?", "help", "This text.", 0},
    {CMD_VERSION, "", "version", "Displays the current version number.", 0}
};

#define command_list_count sizeof(command_list) / sizeof(command_list[0])

CommandLineClass *cmd;
uint8_t tap_version;

struct TAP_BLOCK    // C64 Kernal TAP Block
{
    uint8_t first_countdown_sequence[9];
    uint8_t data_payload[192];
    uint8_t checksum;
    uint8_t second_countdown_sequence[9];
    uint8_t data_payload_backup[192];
    uint8_t checksum_backup;
};

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

int AnalyzeKernalBlock(TAP_BLOCK *block)
{
    // Check if first countdown sequence is correct
    uint8_t start_countdown_first = 0x89;
    uint8_t start_countdown_second = 0x09;
    for(int i=0; i<9; i++)
    {
        if(block->first_countdown_sequence[i] != start_countdown_first)
        {
            return -1;
        }
        
        if(block->second_countdown_sequence[i] != start_countdown_second)
        {
            return -1;
        }   

        start_countdown_first--;
        start_countdown_second--;
    }

    // Check if checksum is correct
    uint8_t checksum = 0;
    uint8_t checksum_backup = 0;

    for(int i=0; i<192; i++)
    {
        checksum ^= block->data_payload[i];
        checksum_backup ^= block->data_payload_backup[i];
    }   

    if(checksum != block->checksum)
    {
        printf("Checksum error in payload data\n");
        return -1;
    }

    if(checksum_backup != block->checksum_backup)
    {
        printf("Checksum error in payload backup data\n");
        return -1;
    }

    return 0; 
}

bool FindAllKernalBlocks(uint8_t *data, uint32_t size)
{
    std::vector<ByteVector> block_list;

    uint32_t pos = 0X14;    // start data's at position 0x14 in TAP file 
    bool error;
    bool start_new_block;

    ByteVector *current_block;

    while(pos < size)
    {
        uint8_t data_byte = GetNextKernalByte(data, size, pos, error, start_new_block);
        if(!error)
        {
            if(start_new_block)
            {
                // Start new block
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
                printf("Error reading byte at position %4.4x\n",pos);
            }
            else
            {
                printf("End of TAP file reached.\n");
            }
        }  
    }

    printf("BlockLsit size: %ld\n", block_list.size());

    return false;
}

void AnalyzeTAPFile(const char *tap_file)
{
    std::ifstream tap_file_stream(tap_file, std::ios::binary);
    if(tap_file_stream.is_open())
    {
        tap_file_stream.seekg(0, std::ios::end);
        std::streamoff file_size = tap_file_stream.tellg();
        tap_file_stream.seekg(0, std::ios::beg);

        uint8_t *tap_data = new uint8_t[file_size];
        tap_file_stream.read((char*)tap_data, file_size);
        tap_file_stream.close();

        printf("TAP file size: %ld\n", static_cast<long>(file_size));
        
        if(IsTAPFile(tap_data, (uint32_t)file_size))
        {
            printf("TAP file is valid.\n");
            printf("TAP version: %d\n",tap_version);
            FindAllKernalBlocks(tap_data, (uint32_t)file_size);
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