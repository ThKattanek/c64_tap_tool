#include <iostream>
#include <fstream>

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

uint32_t GetNetxtPulse(uint8_t *data, uint32_t size, uint32_t &pos)
{
    uint32_t pulse_length = data[pos];

    if(pulse_length == 0x00)
    {
        if(tap_version == 0)
        {
            pulse_length = 256 * 8;
        }
            
        if(tap_version == 1)
        {
            pulse_length = data[pos+1];
            pos += 3;
        }
    }
    else
    {
        pulse_length *= 8;
    }

    return pulse_length;
}

bool FoundHeader(uint8_t *data, uint32_t size)
{
    uint32_t pos = 0x14;

    u_int32_t sync_start = 0;
    u_int32_t sync_end = 0;
    uint32_t sync_pulse_count = 0;
    bool found_sync = false;

    while(pos < size)
    {
        uint32_t pulse_length = data[pos];

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
            // Short Pulse
            sync_pulse_count++;

            if((sync_pulse_count > 1) && !found_sync)
            {
                sync_start = pos-1;
                found_sync = true;
            }
        }
        else if(pulse_length >= MEDIUM_PULSE_MIN && pulse_length <= MEDIUM_PULSE_MAX)
        {
            // Medium Pulse
            if(found_sync)
            {
                sync_end = pos-1;
                found_sync = false;
                if(sync_end - sync_start >= 30)
                {
                    printf("Found Syncronization: %4.4x - %4.4x (%d pulses)\n",sync_start,sync_end, sync_end - sync_start);
                }
            }
            sync_pulse_count = 0;
        }
        else if(pulse_length >= LONG_PULSE_MIN && pulse_length <= LONG_PULSE_MAX)
        {
            // Long Pulse
            if(found_sync)
            {
                sync_end = pos-1;
                found_sync = false;
                if(sync_end - sync_start >= 60)
                {
                    printf("Found Syncronization: %4.4x - %4.4x (%d pulses)\n",sync_start,sync_end, sync_end - sync_start);
                }
            }
            sync_pulse_count = 0;
        }
        pos++;
    }   

    return 0;
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

            FoundHeader(tap_data, (uint32_t)file_size);
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