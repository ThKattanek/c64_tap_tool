#include <iostream>
#include <fstream>

#define VERSION_STRING "0.1"

#include "command_line_class.h"

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

void AnalyzeTAPFile(const char *tap_file)
{
    std::ifstream tap_file_stream(tap_file, std::ios::binary);
    if(tap_file_stream.is_open())
    {
        tap_file_stream.seekg(0, std::ios::end);
        std::streamoff file_size = tap_file_stream.tellg();
        tap_file_stream.seekg(0, std::ios::beg);

        char *tap_data = new char[file_size];
        tap_file_stream.read(tap_data, file_size);
        tap_file_stream.close();

        printf("TAP file size: %d\n",file_size);

        delete[] tap_data;
    }
    else
    {
        printf("Error opening TAP file: %s\n",tap_file);
    }
}