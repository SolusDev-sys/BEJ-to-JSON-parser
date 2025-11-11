/**
 * @file test.cpp
 * @author Vladyslav Kolodii
 * @brief Handles Gtests
 * @version 0.1
 * @date 2025-11-11
 * 
 * @copyright Copyright (c) 2025
 * 
 */
#include <gtest/gtest.h>
extern "C" {
#include "decode.h"
}

// -------------------------
// Utility Function Tests
// -------------------------

TEST(UtilityTests, GetMSB4_ReturnsCorrectBits) 
{
    EXPECT_EQ(get_msb4(0xF0), 0x0F);
    EXPECT_EQ(get_msb4(0xA5), 0x0A);
    EXPECT_EQ(get_msb4(0x00), 0x00);
    EXPECT_EQ(get_msb4(0x7F), 0x07);
}

TEST(BufferTests, ReadAndEOF_Behavior) 
{
    uint8_t data[] = {1, 2, 3, 4};
    BufferReader_t reader;
    init_buffer_reader(&reader, data, sizeof(data));

    uint8_t buffer[2];
    EXPECT_EQ(buffer_read(&reader, buffer, 2), 2);
    EXPECT_FALSE(buffer_eof(&reader));

    EXPECT_EQ(buffer_read(&reader, buffer, 2), 2);
    EXPECT_TRUE(buffer_eof(&reader));

    EXPECT_EQ(buffer_read(&reader, buffer, 2), 0); // end reached
}

// -------------------------
// NNINT Reader Test
// -------------------------

TEST(NNINTTests, ReadNNINTFromBuffer_ValidCases) 
{
    // length=2, value=0x3412 (little endian)
    uint8_t buf[] = {2, 0x12, 0x34};
    BufferReader_t reader;
    init_buffer_reader(&reader, buf, sizeof(buf));

    uint32_t val = 0;
    EXPECT_TRUE(read_nnint_from_buffer(&reader, &val));
    EXPECT_EQ(val, 0x3412);
}

TEST(NNINTTests, ReadNNINTFromBuffer_InvalidLength) 
{
    uint8_t buf[] = {5, 0xAA}; // invalid length (5 > 4)
    BufferReader_t reader;
    init_buffer_reader(&reader, buf, sizeof(buf));

    uint32_t val;
    EXPECT_FALSE(read_nnint_from_buffer(&reader, &val));
}

// -------------------------
// SFLV Reader Test
// -------------------------

TEST(SFLVTests, ReadSFLVFromBuffer_Basic) 
{
    // NNINT (seq=0x02) -> length=1,value=0x04
    // format=0x30 (format=3)
    // NNINT (length=1,value=2)
    // value= {0xAA,0xBB}
    uint8_t buf[] = {1, 0x04, 0x30, 1, 0x02, 0xAA, 0xBB};
    BufferReader_t reader;
    init_buffer_reader(&reader, buf, sizeof(buf));

    SFLV_t sflv;
    EXPECT_TRUE(read_sflv_from_buffer(&reader, &sflv));
    EXPECT_EQ(sflv.sequence, 2);
    EXPECT_EQ(sflv.format, 3);
    EXPECT_EQ(sflv.length, 2u);
    EXPECT_NE(sflv.value, nullptr);

    free_sflv(&sflv);
}

// -------------------------
// Decode Logic Tests
// -------------------------

TEST(DecodeTests, DecodeInteger_PositiveValue) 
{
    uint8_t bytes[] = {0x39, 0x30, 0x00, 0x00};
    SFLV_t sflv = {0, 0, BEJ_FORMAT_INTEGER, 4, bytes};
    DecoderContext_t ctx;
    FILE* out = tmpfile();
    init_decoder_context(&ctx, nullptr, nullptr, nullptr, out);

    EXPECT_TRUE(decode_integer(&ctx, &sflv));
    rewind(out);

    char buffer[32];
    fread(buffer, 1, sizeof(buffer), out);
    fclose(out);
    EXPECT_NE(strstr(buffer, "12345"), nullptr);
}

TEST(DecodeTests, DecodeBoolean_TrueFalse) 
{
    uint8_t val_true[] = {1};
    uint8_t val_false[] = {0};

    SFLV_t sflv_true = {0, 0, BEJ_FORMAT_BOOLEAN, 1, val_true};
    SFLV_t sflv_false = {0, 0, BEJ_FORMAT_BOOLEAN, 1, val_false};

    DecoderContext_t ctx;
    FILE* out = tmpfile();
    init_decoder_context(&ctx, nullptr, nullptr, nullptr, out);

    EXPECT_TRUE(decode_boolean(&ctx, &sflv_true));
    fprintf(out, " ");
    EXPECT_TRUE(decode_boolean(&ctx, &sflv_false));
    rewind(out);

    char buf[32];
    fread(buf, 1, sizeof(buf), out);
    fclose(out);
    EXPECT_NE(strstr(buf, "true"), nullptr);
    EXPECT_NE(strstr(buf, "false"), nullptr);
}

TEST(DecodeTests, DecodeString_Basic) 
{
    uint8_t text[] = {'H','i'};
    SFLV_t sflv = {0, 0, BEJ_FORMAT_STRING, 2, text};
    DecoderContext_t ctx;
    FILE* out = tmpfile();
    init_decoder_context(&ctx, nullptr, nullptr, nullptr, out);

    EXPECT_TRUE(decode_string(&ctx, &sflv));
    rewind(out);

    char buf[16];
    fread(buf, 1, sizeof(buf), out);
    fclose(out);
    EXPECT_NE(strstr(buf, "\"Hi\""), nullptr);
}

// -------------------------
// Decode Dispatcher Test
// -------------------------

TEST(DecodeTests, DecodeValue_DispatchInteger) 
{
    SFLV_t sflv = {0, 0, BEJ_FORMAT_INTEGER, 1, new uint8_t[1]{0x2A}};
    DecoderContext_t ctx;
    FILE* out = tmpfile();
    init_decoder_context(&ctx, nullptr, nullptr, nullptr, out);

    EXPECT_TRUE(decode_value(&ctx, &sflv, nullptr));
    rewind(out);

    char buf[32];
    fread(buf, 1, sizeof(buf), out);
    fclose(out);
    delete[] sflv.value;

    EXPECT_NE(strstr(buf, "42"), nullptr);
}