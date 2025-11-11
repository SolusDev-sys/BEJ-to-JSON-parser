/**
 * @file main.c
 * @author Vladyslav Kolodii
 * @brief Entry point for the BEJ-to-JSON converter command-line utility.
 * @version 0.1
 * @date 2025-11-11
 * 
 * @copyright Copyright (c) 2025
 * 
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "decode.h"

typedef struct
{
    char* schemaDictionary;
    char* annotationDictionary;
    char* bejEncodedFile;
    int verbose;
} DecodeArgs_t;

typedef enum
{
    CMD_DECODE,
    CMD_UNKNOWN
} CommandType_t;

void print_help(const char* program_name);
CommandType_t get_command_type(const char* command);
int validate_parse_filePath(int argc, char* argv[], int current_index, const char* option_name);
int parse_decode_args(int argc, char* argv[], DecodeArgs_t* args);
void BEJ_decode(DecodeArgs_t* args);

int main(int argc, char* argv[])
{
    if (argc < 2) 
    {
        print_help(argv[0]);
        return 1;
    }

    CommandType_t cmd_type = get_command_type(argv[1]);

    if (cmd_type == CMD_UNKNOWN) 
    {
        fprintf(stderr, "Error: Unknown command '%s'\n", argv[1]);
        printf("\n");
        print_help(argv[0]);
        return 1;
    }

    switch (cmd_type) 
    {        
        case CMD_DECODE:
        {
            DecodeArgs_t args;
            if (!parse_decode_args(argc, argv, &args))
            {
                printf("\n");
                return 1;
            }
            BEJ_decode(&args);
            break;
        }
        
        default:
            fprintf(stderr, "Error: Unknown command\n");
            return 1;
    }

    return 0;
}

void print_help(const char* program_name)
{
    printf("USAGE:\n"
           "%s <command> [options] <filename> [options] <filename> [options] <filename>\n\n"
           "COMMANDS:\n"
           "  <decode>\n"
           "    OPTIONS:\n"
           "      -s <file>     Schema dictionary file\n"
           "      -a <file>     Annotation dictionary file\n"
           "      -b <file>     BEJ encoded file for decoding\n"
           "    OPTIONAL ARGUMENTS:\n"
           "      -v            Verbose\n", 
           program_name);
}

CommandType_t get_command_type(const char* command) 
{
    if (strcmp(command, "decode") == 0) 
    {
        return CMD_DECODE;
    }
    return CMD_UNKNOWN;
}

int validate_parse_filePath(int argc, char* argv[], int current_index, const char* option_name)
{
    if (current_index + 1 >= argc) 
    {
        fprintf(stderr, "Error: %s requires a file path\n", option_name);
        return 0;
    }

    if (argv[current_index + 1][0] == '-') 
    {
        fprintf(stderr, "Error: %s requires a file path, got '%s' instead\n", 
                option_name, argv[current_index + 1]);
        return 0;
    }
    return 1;
}

int parse_decode_args(int argc, char* argv[], DecodeArgs_t* args) 
{
    args->schemaDictionary = NULL;
    args->annotationDictionary = NULL;
    args->bejEncodedFile = NULL;
    args->verbose = 0;
    
    for (int i = 2; i < argc; i++) 
    {
        if (strcmp(argv[i], "-s") == 0) 
        {
            if(!validate_parse_filePath(argc, argv, i, "-s"))
                return 0;
            args->schemaDictionary = argv[i + 1];
            i++;
        }
        else if (strcmp(argv[i], "-a") == 0) 
        {
            if(!validate_parse_filePath(argc, argv, i, "-a"))
                return 0;
            args->annotationDictionary = argv[i + 1];
            i++;
        }
        else if (strcmp(argv[i], "-b") == 0) 
        {
            if(!validate_parse_filePath(argc, argv, i, "-b"))
                return 0;
            args->bejEncodedFile = argv[i + 1];
            i++;
        }
        else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0)
        {
            args->verbose = 1;
        }
        else 
        {
            fprintf(stderr, "Error: Unknown option '%s' for <decode> command\n", argv[i]);
            return 0;
        }
    }
    
    if (args->schemaDictionary == NULL) 
    {
        fprintf(stderr, "Error: decode requires -s (schema dictionary)\n");
        return 0;
    }
    if (args->annotationDictionary == NULL) 
    {
        fprintf(stderr, "Error: decode requires -a (annotation dictionary)\n");
        return 0;
    }
    if (args->bejEncodedFile == NULL) 
    {
        fprintf(stderr, "Error: decode requires -b (BEJ encoded file)\n");
        return 0;
    }

    return 1;
}

void BEJ_decode(DecodeArgs_t* args)
{
    if (args->verbose) 
    {
        printf("=== BEJ Decoder Starting ===\n");
        printf("Schema Dictionary: %s\n", args->schemaDictionary);
        printf("Annotation Dictionary: %s\n", args->annotationDictionary);
        printf("BEJ Encoded File: %s\n", args->bejEncodedFile);
    }
    
    // Create output filename by replacing extension with .json
    char output_filename[512];
    const char* input_filename = args->bejEncodedFile;
    
    // Find the last dot in the filename
    const char* last_dot = strrchr(input_filename, '.');
    const char* last_slash = strrchr(input_filename, '/');
    const char* last_backslash = strrchr(input_filename, '\\');
    
    // Determine which path separator came last
    const char* last_separator = last_slash > last_backslash ? last_slash : last_backslash;
    
    // Only use the dot if it comes after the last path separator (i.e., it's part of the filename, not directory)
    if (last_dot && (!last_separator || last_dot > last_separator)) {
        size_t base_len = last_dot - input_filename;
        if (base_len >= sizeof(output_filename) - 6) {
            base_len = sizeof(output_filename) - 6;
        }
        memcpy(output_filename, input_filename, base_len);
        strcpy(output_filename + base_len, ".json");
    } else {
        // No extension found, just append .json
        snprintf(output_filename, sizeof(output_filename), "%s.json", input_filename);
    }
    
    if (args->verbose) 
    {
        printf("Output File: %s\n", output_filename);
        printf("Starting decode process...\n");
    }
    
    // Call the decode function from decode module
    if (!bej_decode_file(args->bejEncodedFile, output_filename,
                         args->schemaDictionary, args->annotationDictionary))
    {
        fprintf(stderr, "Decoding failed\n");
    } 
    else 
    {
        if (args->verbose) 
        {
            printf("=== Decoding Complete ===\n");
        }
    }
}