#include "decode.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

uint8_t get_msb4(uint8_t value)
{
    return (value >> 4) & 0x0F;
}

// ============================================================================
// Dictionary Functions
// ============================================================================

Dictionary_t* load_dictionary(const char* filename)
{
    if (!filename) 
    {
        fprintf(stderr, "Error: Dictionary filename is NULL\n");
        return NULL;
    }
    
    FILE* fp = fopen(filename, "rb");

    if (!fp) 
    {
        fprintf(stderr, "Error: Cannot open dictionary file %s\n", filename);
        return NULL;
    }
    
    Dictionary_t* dict = (Dictionary_t*)calloc(1, sizeof(Dictionary_t));

    if (!dict) 
    {
        fprintf(stderr, "Error: Failed to allocate dictionary memory\n");
        fclose(fp);
        return NULL;
    }
    
    // Read dictionary header: Version (1 byte), Flags (1 byte), EntryCount (2 bytes)
    if (fread(&dict->version_tag, 1, 1, fp) != 1) 
    {
        fprintf(stderr, "Error: Failed to read dictionary format version tag\n");
        free(dict);
        fclose(fp);
        return NULL;
    }
    
    if (fread(&dict->dictionary_flags, 1, 1, fp) != 1) 
    {
        fprintf(stderr, "Error: Failed to read dictionary flags\n");
        free(dict);
        fclose(fp);
        return NULL;
    }
    
    if (fread(&dict->entry_count, 1, 2, fp) != 2) 
    {
        fprintf(stderr, "Error: Failed to read entry count\n");
        free(dict);
        fclose(fp);
        return NULL;
    }

    if (fread(&dict->schema_version, 1, 4, fp) != 4) 
    {
        fprintf(stderr, "Error: Failed to read schema version\n");
        free(dict);
        fclose(fp);
        return NULL;
    }

    if (fread(&dict->dictionary_size, 1, 4, fp) != 4) 
    {
        fprintf(stderr, "Error: Failed to read dictionary size\n");
        free(dict);
        fclose(fp);
        return NULL;
    }

    fprintf(stdout, 
            "Version tag: 0x%02x\n"
            "Dictionary flags: 0x%02x\n"
            "Entry count: %u\n"
            "Schema version: 0x%08X\n"
            "Dictionary size (bytes): %u\n", 
            dict->version_tag, dict->dictionary_flags, dict->entry_count,
            dict->schema_version, dict->dictionary_size
    );
    
    long entries_start = ftell(fp);
    long file_size = dict->dictionary_size;
    uint8_t* file_data = (uint8_t*)malloc(file_size);
    
    if (!file_data) 
    {
        fprintf(stderr, "Error: Failed to allocate file buffer\n");
        free(dict);
        fclose(fp);
        return NULL;
    }

    fseek(fp, 0, SEEK_SET);

    if (fread(file_data, 1, file_size, fp) != (size_t)file_size) 
    {
        fprintf(stderr, "Error: Failed to read dictionary file\n");
        free(file_data);
        free(dict);
        fclose(fp);
        return NULL;
    }
    
    fclose(fp);
    
    // Allocate entries
    dict->entries = (DictionaryEntry_t*)calloc(dict->entry_count, sizeof(DictionaryEntry_t));
    if (!dict->entries) 
    {
        fprintf(stderr, "Error: Failed to allocate dictionary entries\n");
        free(file_data);
        free(dict);
        return NULL;
    }
    
    // Parse entries
    long pos = entries_start;
    for (uint32_t i = 0; i < dict->entry_count; i++) 
    {
        DictionaryEntry_t* entry = &dict->entries[i];

        entry->format = file_data[pos];
        entry->sequence_number = file_data[pos+1] | (file_data[pos+2] << 8);
        entry->child_pointer_offset = file_data[pos+3] | (file_data[pos+4] << 8);
        entry->child_count = file_data[pos+5] | (file_data[pos+6] << 8);
        entry->name_length = file_data[pos+7];
        entry->name_offset = file_data[pos+8] | (file_data[pos+9] << 8);
        
        // Each entry is 10 bytes: format(1) + sequence_number(2) + child_pointer_offset(2) 
        //                         + child_count(2) + name_length (1) + name_offset (2)
        pos += 10;
        
        uint16_t name_pos = entry->name_offset;

       
        if (name_pos >= 0 && name_pos < file_size) 
        {
            uint8_t name_len = entry->name_length;
            if (entry->name_length > 0 && name_len < 255) 
            {
                entry->name = (char*)malloc(name_len + 1);
                if (entry->name) 
                {
                    memcpy(entry->name, &file_data[name_pos], name_len);
                    entry->name[name_len] = '\0';
                }
                else
                {
                    fprintf(stderr, "Error: Failed to allocate file buffer\n");
                }
            } 
            else 
            {
                entry->name = NULL;
            }
        } 
        else 
        {
            entry->name = NULL;
        }
    }
    free(file_data);
    return dict;
}

void free_dictionary(Dictionary_t* dict)
{
    if (!dict) return;
    
    if (dict->entries) 
    {
        for (uint16_t i = 0; i < dict->entry_count; i++) 
        {
            if (dict->entries[i].name) 
            {
                free(dict->entries[i].name);
                dict->entries[i].name = NULL;
            }
        }
        free(dict->entries);
        dict->entries = NULL;
    }
    free(dict);
}

DictionaryEntry_t* find_dictionary_entry(Dictionary_t* dict, DictionaryEntry_t* parent, uint32_t sequence, int8_t format)
{
    if (!dict || !dict->entries) 
    {
        return NULL;
    }
    
    uint32_t start_index = 0;
    uint32_t search_count = dict->entry_count;

    if (parent) 
    {
        const uint32_t DICTIONARY_HEADER_SIZE = 12;
        const uint32_t DICTIONARY_ENTRY_SIZE = 10;

        // Convert byte offset to entry index
        start_index = (parent->child_pointer_offset - DICTIONARY_HEADER_SIZE) / DICTIONARY_ENTRY_SIZE;
        search_count = parent->child_count;
    }
    
    for (uint32_t i = 0; i < search_count; i++) 
    {
        uint32_t index = start_index + i;
    
        if (dict->entries[index].sequence_number == sequence)
        {
             // If format is -1, skip format check (search by sequence only)
            if (format == -1) 
            {
                return &dict->entries[index];
            }

            uint8_t msb4_format = get_msb4(dict->entries[index].format);
            if(msb4_format == format) 
            {
                return &dict->entries[index];
            }
        }
    }
    
    return NULL;
}



// ============================================================================
// Buffer Reader Functions (Cross-platform replacement for fmemopen)
// ============================================================================

void init_buffer_reader(BufferReader_t* reader, uint8_t* data, uint32_t size)
{
    if (!reader) return;
    
    reader->data = data;
    reader->size = size;
    reader->position = 0;
}

uint32_t buffer_read(BufferReader_t* reader, void* dest, uint32_t count)
{
    if (!reader || !dest || reader->position >= reader->size) 
    {
        return 0;
    }
    
    uint32_t available = reader->size - reader->position;
    uint32_t to_read = (count < available) ? count : available;
    
    if (to_read > 0) 
    {
        memcpy(dest, reader->data + reader->position, to_read);
        reader->position += to_read;
    }
    
    return to_read;
}

bool buffer_eof(BufferReader_t* reader)
{
    if (!reader) return true;
    return reader->position >= reader->size;
}

// ============================================================================
// NNINT (Non-Negative Integer) Functions
// ============================================================================

bool read_nnint(FILE* fp, uint32_t* value)
{
    if (!fp || !value)
    {
        return false;
    }

    uint8_t length;
    if (fread(&length, 1, 1, fp) != 1)
    {
        return false;
    }

    if (length == 0 || length > 4) 
    {
        fprintf(stderr, "Error: Invalid NNINT length (%u)\n", length);
        return false;
    }

    uint8_t bytes[4] = {0};
    if (fread(bytes, 1, length, fp) != length) 
    {
        fprintf(stderr, "Error: Failed to read NNINT data bytes\n");
        return false;
    }

    // Combine bytes as little-endian integer
    uint32_t result = 0;
    for (uint8_t i = 0; i < length; ++i) 
    {
        result |= ((uint32_t)bytes[i]) << (8 * i);
    }

    *value = result;
    return true;
}

bool read_nnint_from_buffer(BufferReader_t* reader, uint32_t* value)
{
    if (!reader || !value) 
    {
        return false;
    }

    uint8_t length;
    if (buffer_read(reader, &length, 1) != 1)
    {
        return false;
    }

    if (length == 0 || length > 4) 
    {
        fprintf(stderr, "Error: Invalid NNINT length (%u)\n", length);
        return false;
    }

    uint8_t bytes[4] = {0};
    if (buffer_read(reader, &bytes, length) != length) 
    {
        fprintf(stderr, "Error: Failed to read NNINT data bytes\n");
        return false;
    }

    // Combine bytes as little-endian integer
    uint32_t result = 0;
    for (uint8_t i = 0; i < length; ++i) 
    {
        result |= ((uint32_t)bytes[i]) << (8 * i);
    }

    *value = result;
    return true;
}


// ============================================================================
// SFLV Functions
// ============================================================================

bool read_sflv(FILE* fp, SFLV_t* sflv)
{
    if (!fp || !sflv) 
    {
        return false;
    }
    
    // Read sequence number (nnint) (5.3.6)
    if (!read_nnint(fp, &sflv->sequence)) 
    {
        fprintf(stderr, "Error: Failed to read SFLV sequence\n");
        return false;
    }

    // Dictionary encodings do not include the dictionary selector flag bit. 
    // Separate it from sequence value itself
    sflv->dict_selector = sflv->sequence & 0x1;
    sflv->sequence = sflv->sequence >> 1;
    
    // Read format byte (5.3.7)
    if (fread(&sflv->format, 1, 1, fp) != 1) 
    {
        fprintf(stderr, "Error: Failed to read SFLV format\n");
        return false;
    }

    // Write only the principal data type
    sflv->format = (sflv->format >> 4) & 0x0F;
    
    // Read length (nnint) (5.3.8)
    if (!read_nnint(fp, &sflv->length)) 
    {
        fprintf(stderr, "Error: Failed to read SFLV length\n");
        return false;
    }
    
    // Allocate and read value (5.3.9)
    if (sflv->length > 0) 
    {
        sflv->value = (uint8_t*)malloc(sflv->length);
        if (!sflv->value) 
        {
            fprintf(stderr, "Error: Failed to allocate SFLV value memory\n");
            return false;
        }
        if (fread(sflv->value, 1, sflv->length, fp) != sflv->length) 
        {
            fprintf(stderr, "Error: Failed to read SFLV value\n");
            free(sflv->value);
            sflv->value = NULL;
            return false;
        }
    } 
    else 
    {
        sflv->value = NULL;
    }
    printf("SFLV: seq=%u, format=0x%02X, length=%u, value=0x%X, dict_selector=%u\n", 
            sflv->sequence, sflv->format, sflv->length, sflv->value, sflv->dict_selector);
    return true;
}

bool read_sflv_from_buffer(BufferReader_t* reader, SFLV_t* sflv)
{
    if (!reader || !sflv) 
    {
        return false;
    }
    
    // Read sequence number (nnint) (5.3.6)
    if (!read_nnint_from_buffer(reader, &sflv->sequence)) 
    {
        fprintf(stderr, "Error: Failed to read SFLV sequence from buffer\n");
        return false;
    }

    // Dictionary encodings do not include the dictionary selector flag bit. 
    // Separate it from sequence value itself
    sflv->dict_selector = sflv->sequence & 0x1;
    sflv->sequence = sflv->sequence >> 1;

    // Read format byte (5.3.7)
    if (buffer_read(reader, &sflv->format, 1) != 1) 
    {
        fprintf(stderr, "Error: Failed to read SFLV format from buffer\n");
        return false;
    }

    // Write only the principal data type
    sflv->format = (sflv->format >> 4) & 0x0F;
    
    // Read length (nnint) (5.3.8)
    if (!read_nnint_from_buffer(reader, &sflv->length)) 
    {
        fprintf(stderr, "Error: Failed to read SFLV length from buffer\n");
        return false;
    }
    
    // Allocate and read value (5.3.9)
    if (sflv->length > 0) 
    {
        sflv->value = (uint8_t*)malloc(sflv->length);
        if (!sflv->value) 
        {
            fprintf(stderr, "Error: Failed to allocate SFLV value memory\n");
            return false;
        }
        if (buffer_read(reader, sflv->value, sflv->length) != sflv->length) 
        {
            fprintf(stderr, "Error: Failed to read SFLV value from buffer\n");
            free(sflv->value);
            sflv->value = NULL;
            return false;
        }
    } 
    else 
    {
        sflv->value = NULL;
    }
    printf("SFLV_b: seq=%u, format=0x%02X, length=%u, value=0x%X, dict_selector=%u\n", 
            sflv->sequence, sflv->format, sflv->length, sflv->value, sflv->dict_selector);
    return true;
}

void free_sflv(SFLV_t* sflv)
{
    if (sflv && sflv->value) 
    {
        free(sflv->value);
        sflv->value = NULL;
    }
}


// ============================================================================
// Utility Functions
// ============================================================================

void write_indent(FILE* fp, int level)
{
    if (!fp) return;
    
    for (int i = 0; i < level; i++) 
    {
        fprintf(fp, "\t");
    }
}

void write_json_string(FILE* fp, const char* str, uint32_t length)
{
    if (!fp || !str) return;
    
    fprintf(fp, "\"");
    for (uint32_t i = 0; i < length; i++) 
    {
        unsigned char c = str[i];
        switch (c) 
        {
            case '"':  fprintf(fp, "\\\""); break;
            case '\\': fprintf(fp, "\\\\"); break;
            case '\b': fprintf(fp, "\\b"); break;
            case '\f': fprintf(fp, "\\f"); break;
            case '\n': fprintf(fp, "\\n"); break;
            case '\r': fprintf(fp, "\\r"); break;
            case '\t': fprintf(fp, "\\t"); break;
            default:
                if (c < 0x20) 
                {
                    fprintf(fp, "\\u%04x", c);
                } 
                else 
                {
                    fprintf(fp, "%c", c);
                }
                break;
        }
    }
    fprintf(fp, "\"");
}

void init_decoder_context(DecoderContext_t* ctx, Dictionary_t* schema_dict,
                         Dictionary_t* anno_dict, FILE* input, FILE* output)
{
    if (!ctx) return;
    
    ctx->schema_dict = schema_dict;
    ctx->anno_dict = anno_dict;
    ctx->input_stream = input;
    ctx->output_stream = output;
    ctx->indent_level = 0;
}

// ============================================================================
// Decode Functions - Specific Types
// ============================================================================

bool decode_integer(DecoderContext_t* ctx, SFLV_t* sflv)
{
    if (!ctx || !ctx->output_stream || !sflv) 
    {
        return false;
    }
    
    int64_t int_value = 0;
    
    if (sflv->length > 0 && sflv->length <= 8) 
    {
        // Check if negative (MSB of last byte set)
        bool is_negative = (sflv->value[sflv->length - 1] & 0x80) != 0;
        
        // Reconstruct integer (little-endian)
        for (uint32_t i = 0; i < sflv->length; i++) 
        {
            int_value |= ((int64_t)sflv->value[i]) << (i * 8);
        }
        
        // Sign extend if negative
        if (is_negative && sflv->length < 8) 
        {
            // Create sign extension mask
            uint64_t sign_mask = ~((1ULL << (sflv->length * 8)) - 1);
            int_value |= (int64_t)sign_mask;
        }
    }
    
    fprintf(ctx->output_stream, "%lld", (long long)int_value);
    return true;
}

bool decode_string(DecoderContext_t* ctx, SFLV_t* sflv)
{
    if (!ctx || !ctx->output_stream || !sflv) 
    {
        return false;
    }
    
    if (sflv->value && sflv->length > 0) 
    {
        write_json_string(ctx->output_stream, (const char*)sflv->value, sflv->length);
    } 
    else 
    {
        fprintf(ctx->output_stream, "\"\"");
    }
    
    return true;
}

bool decode_real(DecoderContext_t* ctx, SFLV_t* sflv)
{
    if (!ctx || !ctx->output_stream || !sflv) 
    {
        return false;
    }
    
    if (sflv->length == 4 && sflv->value) 
    {
        // 32-bit float
        float f_value;
        memcpy(&f_value, sflv->value, 4);
        fprintf(ctx->output_stream, "%.7g", f_value);
    } 
    else if (sflv->length == 8 && sflv->value) 
    {
        // 64-bit double
        double d_value;
        memcpy(&d_value, sflv->value, 8);
        fprintf(ctx->output_stream, "%.15g", d_value);
    } 
    else if (sflv->length == 1 && sflv->value) 
    {
        // Some encodings use 1-byte REAL
        fprintf(ctx->output_stream, "%u", (unsigned int)sflv->value[0]);
    } 
    else if (sflv->length == 2 && sflv->value) 
    {
        // 2-byte value - could be half-precision float
        uint16_t val = sflv->value[0] | (sflv->value[1] << 8);
        fprintf(ctx->output_stream, "%u", val);
    } 
    else 
    {
        // Unknown length - output null
        fprintf(ctx->output_stream, "null");
    }
    
    return true;
}

bool decode_boolean(DecoderContext_t* ctx, SFLV_t* sflv)
{
    if (!ctx || !ctx->output_stream || !sflv) 
    {
        return false;
    }
    
    bool value = false;
    if (sflv->length > 0 && sflv->value) 
    {
        value = (sflv->value[0] != 0);
    }
    
    fprintf(ctx->output_stream, "%s", value ? "true" : "false");
    return true;
}

bool decode_enum(DecoderContext_t* ctx, SFLV_t* sflv, DictionaryEntry_t* entry)
{
    if (!ctx || !ctx->output_stream || !sflv) 
    {
        return false;
    }
    
    // Enum value is encoded as nnint - the sequence number for the enumeration option (5.3.12)
    uint32_t enum_sequence = 0;
    
    if (sflv->length > 0 && sflv->value) 
    {
        BufferReader_t reader;
        init_buffer_reader(&reader, sflv->value, sflv->length);
        
        if (!read_nnint_from_buffer(&reader, &enum_sequence)) 
        {
            fprintf(stderr, "Error: Failed to read enum sequence\n");
            fprintf(ctx->output_stream, "null");
            return false;
        }
    }
    
    // Look up the enum option name from the dictionary
    // Use the appropriate dictionary based on dict_selector
    DictionaryEntry_t* enum_entry = NULL;
    if (sflv->dict_selector == 0) 
    {
        enum_entry = find_dictionary_entry(ctx->schema_dict, entry, enum_sequence, -1);
    } 
    else if (sflv->dict_selector == 1) 
    {
        enum_entry = find_dictionary_entry(ctx->anno_dict, entry, enum_sequence, -1);
    }

    if (enum_entry && enum_entry->name) 
    {
        fprintf(ctx->output_stream, "\"%s\"", enum_entry->name);
    } 
    else 
    {
        fprintf(ctx->output_stream, "\"%u\"", enum_sequence);
    }
    
    return true;
}

bool decode_null(DecoderContext_t* ctx)
{
    if (!ctx || !ctx->output_stream) 
    {
        return false;
    }
    
    fprintf(ctx->output_stream, "null");
    return true;
}

bool decode_set(DecoderContext_t* ctx, SFLV_t* sflv, DictionaryEntry_t* entry)
{
    if (!ctx || !ctx->output_stream || !sflv) 
    {
        return false;
    }
    
    fprintf(ctx->output_stream, "{");
    
    if (sflv->length > 0 && sflv->value) 
    {
        BufferReader_t reader;
        init_buffer_reader(&reader, sflv->value, sflv->length);
        
        fprintf(ctx->output_stream, "\n");
        ctx->indent_level++;

        uint32_t set_length;
        if (!read_nnint_from_buffer(&reader, &set_length)) 
        {
            fprintf(stderr, "Error: Failed to read SET length\n");
            return false;
        }

        bool first = true;
        while (!buffer_eof(&reader)) 
        {
            if (!first) 
            {
                fprintf(ctx->output_stream, ",\n");
            }
            first = false;
            
            SFLV_t child_sflv;
            if (!read_sflv_from_buffer(&reader, &child_sflv)) 
            {
                return false;
            }

            DictionaryEntry_t* child_entry = NULL;
            if (child_sflv.dict_selector == 0) 
            {
                child_entry = find_dictionary_entry(ctx->schema_dict, entry, child_sflv.sequence, child_sflv.format);
            } 
            else if (child_sflv.dict_selector == 1) 
            {
                child_entry = find_dictionary_entry(ctx->anno_dict, entry, child_sflv.sequence, child_sflv.format);
            }

            write_indent(ctx->output_stream, ctx->indent_level);
            
            // Write property name
            if (child_entry && child_entry->name) 
            {
                fprintf(ctx->output_stream, "\"%s\":", child_entry->name);
            }
            else 
            {
                fprintf(ctx->output_stream, "\"seq_%u\":", child_sflv.sequence);
            }
            
            fprintf(ctx->output_stream, " ");
            
            // Decode child value
            if (!decode_value(ctx, &child_sflv, child_entry)) 
            {
                free_sflv(&child_sflv);
                return false;
            }
            free_sflv(&child_sflv);
        }
        ctx->indent_level--;
        fprintf(ctx->output_stream, "\n");
        write_indent(ctx->output_stream, ctx->indent_level);
    }
    fprintf(ctx->output_stream, "}");
    return true;
}

bool decode_array(DecoderContext_t* ctx, SFLV_t* sflv, DictionaryEntry_t* entry)
{
    if (!ctx || !ctx->output_stream || !sflv) 
    {
        return false;
    }
    
    fprintf(ctx->output_stream, "[");
    
    if (sflv->length > 0 && sflv->value) 
    {
        BufferReader_t reader;
        init_buffer_reader(&reader, sflv->value, sflv->length);
                
        uint32_t* array_length;
        if (!read_nnint_from_buffer(&reader, array_length)) 
        {
            fprintf(stderr, "Error: Failed to read SET length\n");
            return false;
        }

        bool first = true;
        
        while (!buffer_eof(&reader)) 
        {
            if (!first)
            {
                fprintf(ctx->output_stream, ", ");
            }
            first = false;
            
            SFLV_t element_sflv;
            if (!read_sflv_from_buffer(&reader, &element_sflv)) 
            {
                return false;
            }
            
            // Decode array element
            if (!decode_value(ctx, &element_sflv, entry)) 
            {
                free_sflv(&element_sflv);
                return false;
            }
            free_sflv(&element_sflv);
        }
    }

    fprintf(ctx->output_stream, "]");
    return true;
}

// ============================================================================
// Main Decode Value Function
// ============================================================================

bool decode_value(DecoderContext_t* ctx, SFLV_t* sflv, DictionaryEntry_t* entry)
{
    if (!ctx || !sflv) 
    {
        return false;
    }
    
    switch (sflv->format) 
    {
        case BEJ_FORMAT_SET:
            return decode_set(ctx, sflv, entry);
            
        case BEJ_FORMAT_ARRAY:
            return decode_array(ctx, sflv, entry);
            
        case BEJ_FORMAT_NULL:
            return decode_null(ctx);

        case BEJ_FORMAT_INTEGER: // check it
            return decode_integer(ctx, sflv);
            
        case BEJ_FORMAT_ENUM:
            return decode_enum(ctx, sflv, entry);
            
        case BEJ_FORMAT_STRING:
            return decode_string(ctx, sflv);
            
        case BEJ_FORMAT_REAL: // check it
            return decode_real(ctx, sflv);
            
        case BEJ_FORMAT_BOOLEAN:
            return decode_boolean(ctx, sflv);
                     
        case BEJ_FORMAT_BYTE_STRING:
            // Byte string could be base64 encoded
            fprintf(ctx->output_stream, "\"<byte_string>\"");
            return true;
            
        case BEJ_FORMAT_CHOICE:
        case BEJ_FORMAT_PROPERTY_ANNOTATION:
        case BEJ_FORMAT_REGISTRY_ITEM:
            fprintf(stderr, "Warning: Format type 0x%02X not fully implemented\n", 
                    sflv->format);
            fprintf(ctx->output_stream, "null");
            return true;
            
        default:
            fprintf(stderr, "Error: Unknown format type 0x%02X\n", sflv->format);
            fprintf(ctx->output_stream, "null");
            return false;
    }
}

// ============================================================================
// Main Decode Function
// ============================================================================

bool decode_bej_to_json(DecoderContext_t* ctx)
{
    if (!ctx || !ctx->input_stream || !ctx->output_stream) 
    {
        fprintf(stderr, "Error: Invalid decoder context\n");
        return false;
    }
    
    // Read BEJ version header (4 bytes) (5.3.4)
    // Version is 32-bit: 0xF1F0F000 (v1.0.0) or 0xF1F1F000 (v1.1.0)
    uint8_t version_bytes[4];
    if (fread(version_bytes, 1, 4, ctx->input_stream) != 4) 
    {
        fprintf(stderr, "Error: Failed to read BEJ version\n");
        return false;
    }
    uint32_t version = version_bytes[0] | (version_bytes[1] << 8) 
                      | (version_bytes[2] << 16) | (version_bytes[3] << 24);
    fprintf(stderr, "BEJ Version: 0x%08X\n", version);

    // Read BEJ flags (2 bytes) (5.3.4)
    uint8_t BEG_flags_bytes[2];
    if (fread(BEG_flags_bytes, 1, 2, ctx->input_stream) != 2) 
    {
        fprintf(stderr, "Error: Failed to read BEJ flags\n");
        return false;
    }
    uint16_t BEG_flags = BEG_flags_bytes[0] | (BEG_flags_bytes[1] << 8);
    fprintf(stderr, "BEJ Flags: 0x%04X\n", BEG_flags);

    // Read schemaClass (1 byte) (5.3.2)
    uint8_t schemaClass_bytes[1];
    if (fread(schemaClass_bytes, 1, 1, ctx->input_stream) != 1) 
    {
        fprintf(stderr, "Error: Failed to read Schema Class\n");
        return false;
    }
    uint8_t schemaClass = schemaClass_bytes[0];
    fprintf(stderr, "Schema class: 0x%02X\n", schemaClass);
    
    SFLV_t sflv;

    if (!read_sflv(ctx->input_stream, &sflv))
    {
        fprintf(stderr, "Error: Failed to read SFLV tuple\n");
        return false;
    }

    bool result = decode_value(ctx, &sflv, NULL);
    
    // Add final newline
    fprintf(ctx->output_stream, "\n");

    // Flush output to ensure it's written
    fflush(ctx->output_stream);
    
    free_sflv(&sflv);
    
    if (result) 
    {
        fprintf(stderr, "Decoding completed successfully\n");
    } 
    else 
    {
        fprintf(stderr, "Decoding failed\n");
    }
    
    return result;
}

// ============================================================================
// High-Level API
// ============================================================================

bool bej_decode_file(const char* input_file, const char* output_file,
                     const char* schema_dict_file, const char* anno_dict_file)
{
    if (!input_file || !output_file || !schema_dict_file || !anno_dict_file) 
    {
        fprintf(stderr, "Error: Invalid parameters\n");
        return false;
    }
    
    fprintf(stderr, "Loading schema dictionary: %s\n", schema_dict_file);
    Dictionary_t* schema_dict = load_dictionary(schema_dict_file);
    if (!schema_dict) 
    {
        fprintf(stderr, "Error: Failed to load schema dictionary\n");
        return false;
    }
    fprintf(stderr, "Schema dictionary loaded: %u entries\n", schema_dict->entry_count);
    //print_dictionary(schema_dict);

    fprintf(stderr, "Loading annotation dictionary: %s\n", anno_dict_file);
    Dictionary_t* anno_dict = load_dictionary(anno_dict_file);
    if (!anno_dict) 
    {
        fprintf(stderr, "Error: Failed to load annotation dictionary\n");
        free_dictionary(schema_dict);
        return false;
    }
    fprintf(stderr, "Annotation dictionary loaded: %u entries\n", anno_dict->entry_count);

    fprintf(stderr, "Opening input file: %s\n", input_file);
    FILE* input = fopen(input_file, "rb");
    if (!input) 
    {
        fprintf(stderr, "Error: Cannot open input file %s\n", input_file);
        free_dictionary(schema_dict);
        free_dictionary(anno_dict);
        return false;
    }
    
    // Check input file size
    fseek(input, 0, SEEK_END);
    long input_size = ftell(input);
    fseek(input, 0, SEEK_SET);
    fprintf(stderr, "Input file size: %ld bytes\n", input_size);
    
    if (input_size == 0) 
    {
        fprintf(stderr, "Error: Input file is empty\n");
        fclose(input);
        free_dictionary(schema_dict);
        free_dictionary(anno_dict);
        return false;
    }
    
    fprintf(stderr, "Creating output file: %s\n", output_file);
    FILE* output = fopen(output_file, "w");
    if (!output) 
    {
        fprintf(stderr, "Error: Cannot create output file %s\n", output_file);
        fclose(input);
        free_dictionary(schema_dict);
        free_dictionary(anno_dict);
        return false;
    }
    
    DecoderContext_t ctx;
    init_decoder_context(&ctx, schema_dict, anno_dict, input, output);
    
    fprintf(stderr, "Starting BEJ decode...\n");
    bool result = decode_bej_to_json(&ctx);
    if (result)
    {
        printf("Successfully decoded BEJ to JSON: %s\n", output_file);
    } 
    else 
    {
        fprintf(stderr, "Error: Failed to decode BEJ file\n");
    }
    
    fclose(input);
    fclose(output);
    free_dictionary(schema_dict);
    free_dictionary(anno_dict);
    return result;
}