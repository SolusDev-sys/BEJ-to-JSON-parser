#ifndef DECODE_H
#define DECODE_H

// Number commented in () are section number

// EXAMPLE "Dictionary entry structure (7.2.3.2)"
// Section number is 7.2.3.2

// For more information checkout DSP0218_1.2.0

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

// BEJ Format constants as per DSP0218 (5.3.7)
#define BEJ_FORMAT_SET                  0x00
#define BEJ_FORMAT_ARRAY                0x01
#define BEJ_FORMAT_NULL                 0x02
#define BEJ_FORMAT_INTEGER              0x03
#define BEJ_FORMAT_ENUM                 0x04
#define BEJ_FORMAT_STRING               0x05
#define BEJ_FORMAT_REAL                 0x06
#define BEJ_FORMAT_BOOLEAN              0x07
#define BEJ_FORMAT_BYTE_STRING          0x08
#define BEJ_FORMAT_CHOICE               0x09
#define BEJ_FORMAT_PROPERTY_ANNOTATION  0x0A
#define BEJ_FORMAT_REGISTRY_ITEM        0x0B

// Buffer reader structure for cross-platform memory reading
typedef struct {
    uint8_t* data;
    uint32_t size;
    uint32_t position;
} BufferReader_t;

// SFLV (Sequence, Format, Length, Value) structure (5.3.6 - 5.3.9)
typedef struct 
{
    uint32_t sequence;
    uint8_t dict_selector;
    uint8_t format; //only 4 MSB bits used
    uint32_t length;
    uint8_t* value;
} SFLV_t;

// Dictionary entry structure (7.2.3.2)
typedef struct 
{
    uint8_t format;
    uint16_t sequence_number;
    uint16_t child_pointer_offset;
    uint16_t child_count;
    uint8_t name_length;
    uint16_t name_offset;
    char* name;
} DictionaryEntry_t;

// Dictionary structure (7.2.3.2)
typedef struct 
{
    DictionaryEntry_t* entries;
    uint8_t version_tag;
    uint8_t dictionary_flags;
    uint16_t entry_count;
    uint32_t schema_version;
    uint32_t dictionary_size;
} Dictionary_t;

// Decoder context
typedef struct {
    Dictionary_t* schema_dict;
    Dictionary_t* anno_dict;
    FILE* input_stream;
    FILE* output_stream;
    int indent_level;
} DecoderContext_t;

// Main decode function
/**
 * Decode a BEJ encoded file to JSON
 * @param input_file Path to BEJ encoded file
 * @param output_file Path to output JSON file
 * @param schema_dict_file Path to schema dictionary file
 * @param anno_dict_file Path to annotation dictionary file
 * @return true on success, false on failure
 */
bool bej_decode_file(const char* input_file, const char* output_file,
                     const char* schema_dict_file, const char* anno_dict_file);

// Dictionary functions
/**
 * Load a BEJ dictionary from file
 * @param filename Path to dictionary file
 * @return Pointer to Dictionary_t or NULL on failure
 */
Dictionary_t* load_dictionary(const char* filename);

/**
 * Free dictionary memory
 * @param dict Dictionary to free
 */
void free_dictionary(Dictionary_t* dict);

void print_dictionary(Dictionary_t* dict);

/**
 * Find dictionary entry by sequence number
 * @param dict Dictionary to search
 * @param sequence Sequence number to find
 * @return Pointer to DictionaryEntry_t or NULL if not found
 */
DictionaryEntry_t* find_dictionary_entry(Dictionary_t* dict, DictionaryEntry_t* parent, uint32_t sequence, int8_t format);

// NNINT (Non-Negative Integer) functions
/**
 * Read NNINT from file stream
 * @param fp File pointer
 * @param value Pointer to store the read value
 * @return true on success, false on failure
 */
bool read_nnint(FILE* fp, uint32_t* value);

/**
 * Read NNINT from buffer
 * @param reader Buffer reader
 * @param value Pointer to store the read value
 * @return true on success, false on failure
 */
bool read_nnint_from_buffer(BufferReader_t* reader, uint32_t* value);

// SFLV functions
/**
 * Read SFLV tuple from BEJ stream
 * @param fp File pointer
 * @param sflv Pointer to SFLV_t structure to fill
 * @return true on success, false on failure
 */
bool read_sflv(FILE* fp, SFLV_t* sflv);

/**
 * Read SFLV tuple from buffer
 * @param reader Buffer reader
 * @param sflv Pointer to SFLV_t structure to fill
 * @return true on success, false on failure
 */
bool read_sflv_from_buffer(BufferReader_t* reader, SFLV_t* sflv);

/**
 * Free SFLV value memory
 * @param sflv SFLV_t structure with allocated value
 */
void free_sflv(SFLV_t* sflv);

// Decoding functions
/**
 * Decode BEJ stream to JSON
 * @param ctx Decoder context
 * @return true on success, false on failure
 */
bool decode_bej_to_json(DecoderContext_t* ctx);

/**
 * Decode a single BEJ value
 * @param ctx Decoder context
 * @param sflv SFLV tuple to decode
 * @param entry Dictionary entry (can be NULL)
 * @return true on success, false on failure
 */
bool decode_value(DecoderContext_t* ctx, SFLV_t* sflv, DictionaryEntry_t* entry);

/**
 * Decode BEJ SET format (JSON object)
 * @param ctx Decoder context
 * @param sflv SFLV tuple with SET data
 * @param entry Dictionary entry
 * @return true on success, false on failure
 */
bool decode_set(DecoderContext_t* ctx, SFLV_t* sflv, DictionaryEntry_t* entry);

/**
 * Decode BEJ ARRAY format
 * @param ctx Decoder context
 * @param sflv SFLV tuple with ARRAY data
 * @param entry Dictionary entry
 * @return true on success, false on failure
 */
bool decode_array(DecoderContext_t* ctx, SFLV_t* sflv, DictionaryEntry_t* entry);

/**
 * Decode BEJ NULL format
 * @param ctx Decoder context
 * @return true on success, false on failure
 */
bool decode_null(DecoderContext_t* ctx);

/**
 * Decode BEJ INTEGER format
 * @param ctx Decoder context
 * @param sflv SFLV tuple with INTEGER data
 * @return true on success, false on failure
 */
bool decode_integer(DecoderContext_t* ctx, SFLV_t* sflv);

/**
 * Decode BEJ ENUM format
 * @param ctx Decoder context
 * @param sflv SFLV tuple with ENUM data
 * @param entry Dictionary entry
 * @return true on success, false on failure
 */
bool decode_enum(DecoderContext_t* ctx, SFLV_t* sflv, DictionaryEntry_t* entry);

/**
 * Decode BEJ STRING format
 * @param ctx Decoder context
 * @param sflv SFLV tuple with STRING data
 * @return true on success, false on failure
 */
bool decode_string(DecoderContext_t* ctx, SFLV_t* sflv);

/**
 * Decode BEJ REAL format (floating point)
 * @param ctx Decoder context
 * @param sflv SFLV tuple with REAL data
 * @return true on success, false on failure
 */
bool decode_real(DecoderContext_t* ctx, SFLV_t* sflv);

/**
 * Decode BEJ BOOLEAN format
 * @param ctx Decoder context
 * @param sflv SFLV tuple with BOOLEAN data
 * @return true on success, false on failure
 */
bool decode_boolean(DecoderContext_t* ctx, SFLV_t* sflv);

// Utility functions
/**
 * Extracts the 4 most significant bits (MSBs) from an 8-bit value.
 * @param value Input byte.
 * @return uint8_t The 4 MSBs of the value (range 0–15).
 */
uint8_t get_msb4(uint8_t value);

/**
 * Initialize buffer reader
 * @param reader Buffer reader to initialize
 * @param data Pointer to buffer data
 * @param size Size of buffer
 */
void init_buffer_reader(BufferReader_t* reader, uint8_t* data, uint32_t size);

/**
 * Read bytes from buffer
 * @param reader Buffer reader
 * @param dest Destination buffer
 * @param count Number of bytes to read
 * @return Number of bytes actually read
 */
uint32_t buffer_read(BufferReader_t* reader, void* dest, uint32_t count);

/**
 * Check if at end of buffer
 * @param reader Buffer reader
 * @return true if at end, false otherwise
 */
bool buffer_eof(BufferReader_t* reader);

/**
 * Write indentation to output
 * @param fp File pointer
 * @param level Indentation level
 */
void write_indent(FILE* fp, int level);

/**
 * Write escaped string to JSON output
 * @param fp File pointer
 * @param str String to write
 * @param length Length of string
 */
void write_json_string(FILE* fp, const char* str, uint32_t length);

/**
 * Initialize decoder context
 * @param ctx Decoder context to initialize
 * @param schema_dict Schema dictionary
 * @param anno_dict Annotation dictionary
 * @param input Input file stream
 * @param output Output file stream
 */
void init_decoder_context(DecoderContext_t* ctx, Dictionary_t* schema_dict,
                         Dictionary_t* anno_dict, FILE* input, FILE* output);

#endif // DECODE_H