#define _POSIX_C_SOURCE 200809L
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <stdbool.h>

#include "ft8/text.h"
#include "ft8/encode.h"
#include "ft8/constants.h"
#include "ft8/hashtable.h"

#include "fft/kiss_fftr.h"
#include "common/common.h"
#include "ft8/message.h"

#define LOG_LEVEL LOG_INFO
#include "ft8/debug.h"

// void convert_8bit_to_6bit(uint8_t* dst, const uint8_t* src, int nBits)
// {
//     // Zero-fill the destination array as we will only be setting bits later
//     for (int j = 0; j < (nBits + 5) / 6; ++j)
//     {
//         dst[j] = 0;
//     }

//     // Set the relevant bits
//     uint8_t mask_src = (1 << 7);
//     uint8_t mask_dst = (1 << 5);
//     for (int i = 0, j = 0; nBits > 0; --nBits)
//     {
//         if (src[i] & mask_src)
//         {
//             dst[j] |= mask_dst;
//         }
//         mask_src >>= 1;
//         if (mask_src == 0)
//         {
//             mask_src = (1 << 7);
//             ++i;
//         }
//         mask_dst >>= 1;
//         if (mask_dst == 0)
//         {
//             mask_dst = (1 << 5);
//             ++j;
//         }
//     }
// }

/*
bool test1() {
    //const char *msg = "CQ DL7ACA JO40"; // 62, 32, 32, 49, 37, 27, 59, 2, 30, 19, 49, 16
    const char *msg = "VA3UG   F1HMR 73"; // 52, 54, 60, 12, 55, 54, 7, 19, 2, 23, 59, 16
    //const char *msg = "RA3Y VE3NLS 73";   // 46, 6, 32, 22, 55, 20, 11, 32, 53, 23, 59, 16
    uint8_t a72[9];

    int rc = packmsg(msg, a72);
    if (rc < 0) return false;

    LOG(LOG_INFO, "8-bit packed: ");
    for (int i = 0; i < 9; ++i) {
        LOG(LOG_INFO, "%02x ", a72[i]);
    }
    LOG(LOG_INFO, "\n");

    uint8_t a72_6bit[12];
    convert_8bit_to_6bit(a72_6bit, a72, 72);
    LOG(LOG_INFO, "6-bit packed: ");
    for (int i = 0; i < 12; ++i) {
        LOG(LOG_INFO, "%d ", a72_6bit[i]);
    }
    LOG(LOG_INFO, "\n");

    char msg_out_raw[14];
    unpack(a72, msg_out_raw);

    char msg_out[14];
    fmtmsg(msg_out, msg_out_raw);
    LOG(LOG_INFO, "msg_out = [%s]\n", msg_out);
    return true;
}


void test2() {
    uint8_t test_in[11] = { 0xF1, 0x02, 0x03, 0x04, 0x05, 0x60, 0x70, 0x80, 0x90, 0xA0, 0xFF };
    uint8_t test_out[22];

    encode174(test_in, test_out);

    for (int j = 0; j < 22; ++j) {
        LOG(LOG_INFO, "%02x ", test_out[j]);
    }
    LOG(LOG_INFO, "\n");
}


void test3() {
    uint8_t test_in2[10] = { 0x11, 0x00, 0x00, 0x00, 0x00, 0x0E, 0x10, 0x04, 0x01, 0x00 };
    uint16_t crc1 = ftx_compute_crc(test_in2, 76);  // Calculate CRC of 76 bits only
    LOG(LOG_INFO, "CRC: %04x\n", crc1);            // should be 0x0708
}
*/

#define CHECK(condition)                                       \
    if (!(condition))                                          \
    {                                                          \
        printf("FAIL: Condition \'" #condition "' failed!\n"); \
        return;                                                \
    }

#define TEST_END printf("Test OK\n\n")


void msg_to_bitstring(ftx_message_t *msg, char *bitstring)
{
    size_t out_pos = 0;
    for (size_t i = 0; i < sizeof(msg->payload); i++)
    {
        for (int8_t j = 7; j >= 0; j--)
        {
            bitstring[out_pos] = ((msg->payload[i] >> j) & 1) ? '1' : '0';
            out_pos++;
            if (out_pos > 76) {
                break;
            }
        }

    }
}

void bitstring_to_msg(const char *bitstring, ftx_message_t *msg)
{
    size_t bit_pos = 0;
    for (size_t i = 0; i < sizeof(msg->payload); i++)
    {
        for (int8_t j = 7; j >= 0; j--)
        {
            msg->payload[i] |= (bitstring[bit_pos] == '1') << j;
            bit_pos++;
            if (bit_pos > strlen(bitstring)) {
                break;
            }
        }

    }
}

void test_std_msg(const char* call_to_tx, const char* call_de_tx, const char* extra_tx)
{
    ftx_message_t msg;
    ftx_message_init(&msg);

    ftx_message_rc_t rc_encode = ftx_message_encode_std(&msg, &hash_if, call_to_tx, call_de_tx, extra_tx);
    CHECK(rc_encode == FTX_MESSAGE_RC_OK);
    printf("Encoded [%s] [%s] [%s]\n", call_to_tx, call_de_tx, extra_tx);

    char call_to[14];
    char call_de[14];
    char extra[14];
    ftx_message_rc_t rc_decode = ftx_message_decode_std(&msg, &hash_if, call_to, call_de, extra);
    if (starts_with(call_to_tx, "CQ_")) {
        call_to[2] = '_';
    }
    CHECK(rc_decode == FTX_MESSAGE_RC_OK);
    printf("Decoded [%s] [%s] [%s]\n", call_to, call_de, extra);
    CHECK(0 == strcmp(call_to, call_to_tx));
    CHECK(0 == strcmp(call_de, call_de_tx));
    CHECK(0 == strcmp(extra, extra_tx));
    // CHECK(1 == 2);
    TEST_END;
}

void test_msg(const char* message_text, const char* expected, ftx_callsign_hash_interface_t *hash_if)
{
    printf("Testing [%s]\n", message_text);

    ftx_message_t msg;
    ftx_message_init(&msg);

    ftx_message_rc_t rc_encode = ftx_message_encode(&msg, hash_if, message_text);
    CHECK(rc_encode == FTX_MESSAGE_RC_OK);

    char message_decoded[12+12+20];
    ftx_message_rc_t rc_decode = ftx_message_decode(&msg, hash_if, message_decoded);
    CHECK(rc_decode == FTX_MESSAGE_RC_OK);
    printf("Decoded [%s]\n", message_decoded);
    CHECK(0 == strcmp(expected, message_decoded));
    // CHECK(1 == 2);
    TEST_END;
}


void test_encoding(const char* message_text, const char* expected_bits, ftx_callsign_hash_interface_t *hash_if)
{
    char bits[78];
    bits[77] = 0;
    printf("Testing encoding [%s] -> [%s]\n", message_text, expected_bits);

    ftx_message_t msg;
    ftx_message_init(&msg);

    ftx_message_rc_t rc_encode = ftx_message_encode(&msg, hash_if, message_text);
    CHECK(rc_encode == FTX_MESSAGE_RC_OK);

    msg_to_bitstring(&msg, bits);
    printf("Encoded bits [%s]\n", bits);
    CHECK(0 == strcmp(expected_bits, bits));
    // CHECK(1 == 2);
    TEST_END;
}

void test_decoding(const char* bits, const char* expected_text, ftx_callsign_hash_interface_t *hash_if)
{
    char decoded_text[32];
    decoded_text[0] = '\0';
    printf("Testing decoding [%s] -> [%s]\n", bits, expected_text);

    ftx_message_t msg;
    ftx_message_init(&msg);
    bitstring_to_msg(bits, &msg);

    ftx_message_rc_t rc_decode = ftx_message_decode(&msg, hash_if, decoded_text);
    CHECK(rc_decode == FTX_MESSAGE_RC_OK);

    printf("Decoded [%s]\n", decoded_text);
    CHECK(0 == strcmp(decoded_text, expected_text));
    // CHECK(1 == 2);
    TEST_END;
}


#define SIZEOF_ARRAY(x) (sizeof(x) / sizeof((x)[0]))

int main()
{
    hashtable_init(256);
    // test1();
    // test4();
    const char* callsigns[] = { "YL3JG", "W1A", "W1A/R", "W5AB", "W8ABC", "DE6ABC", "DE6ABC/R", "DE7AB", "DE9A", "3DA0X", "3DA0XYZ", "3DA0XYZ/R", "3XZ0AB", "3XZ0A"};
    const char* tokens[] = { "CQ", "QRZ", "CQ_123", "CQ_000", "CQ_POTA", "CQ_SA", "CQ_O", "CQ_ASD" };
    const char* grids[] = { "KO26", "RR99", "AA00", "RR09", "AA01", "RRR", "RR73", "73", "R+10", "R+05", "R-12", "R-02", "+10", "+05", "-02", "-02", "" };

    for (int idx_grid = 0; idx_grid < SIZEOF_ARRAY(grids); ++idx_grid)
    {
        for (int idx_callsign = 0; idx_callsign < SIZEOF_ARRAY(callsigns); ++idx_callsign)
        {
            for (int idx_callsign2 = 0; idx_callsign2 < SIZEOF_ARRAY(callsigns); ++idx_callsign2)
            {
                test_std_msg(callsigns[idx_callsign], callsigns[idx_callsign2], grids[idx_grid]);
            }
        }
        for (int idx_token = 0; idx_token < SIZEOF_ARRAY(tokens); ++idx_token)
        {
            for (int idx_callsign2 = 0; idx_callsign2 < SIZEOF_ARRAY(callsigns); ++idx_callsign2)
            {
                test_std_msg(tokens[idx_token], callsigns[idx_callsign2], grids[idx_grid]);
            }
        }
    }
    test_msg("CQ EA8/G5LSI", "CQ EA8/G5LSI", NULL);
    test_msg("EA8/G5LSI R2RFE RR73", "<EA8/G5LSI> R2RFE RR73", &hash_if);
    test_msg("R2RFE/P EA8/G5LSI R+12", "R2RFE/P <EA8/G5LSI> R+12", &hash_if);
    test_msg("CQ SP9HGN/QRP", "CQ SP9HGN/QRP", &hash_if);

    test_msg("EA8/G5LSI SP9HGN/QRP +10", "<EA8/G5LSI> <SP9HGN/QRP> +10", &hash_if);

    test_msg("CQ PD80LDN", "CQ PD80LDN", NULL);

    test_msg("TNX BOB 73 GL", "TNX BOB 73 GL", NULL);
    test_msg("UFX KR0123", "UFX KR0123", NULL);

    // Nonstandard call
    test_encoding("CQ SP9HGN/QRP", "10100001101100000100010110110110001000111000100101000011101110101001100001100", &hash_if);
    test_encoding("CQ EA8/DK5CR", "00011100010000000000000011110001111100100001011000100100100001100110100001100", &hash_if);
    test_encoding("CQ PD80LDN", "01000100011100000000000000000000010010011111010101101001110101111011000001100", &hash_if);
    // Free text
    test_encoding("TNX BOB 73 GL", "01100011111011011100111011100010101001001010111000000111111101010000000000000", NULL);

    // Test for decoding
    test_decoding("01100011111011011100111011100010101001001010111000000111111101010000000000000", "TNX BOB 73 GL", &hash_if);
    test_decoding("00000000000000000000000000100000010011011110111100011010100010100001100110001", "CQ K1ABC FN42", &hash_if);
    test_decoding("00001001101111011110001101010000011000010100100111011100000010000101011001001", "K1ABC W9XYZ EN37", &hash_if);
    test_decoding("00001100001010010011101110000000010011011110111100011010100111111010101000001", "W9XYZ K1ABC -11", &hash_if);
    test_decoding("00001001101111011110001101010000011000010100100111011100001111111010101010001", "K1ABC W9XYZ R-09", &hash_if);
    test_decoding("00001100001010010011101110000000010011011110111100011010100111111010010010001", "W9XYZ K1ABC RRR", &hash_if);
    test_decoding("00001001101111011110001101010000011000010100100111011100000111111010010100001", "K1ABC W9XYZ 73", &hash_if);
    test_decoding("00001001101111011110001101010000011000010100100111011100000111111001110101001", "K1ABC W9XYZ RR73", &hash_if);
    test_decoding("00000000000000000100100100010000010011011110111100011010100010100001100110001", "CQ FD K1ABC FN42", &hash_if);
    test_decoding("00000000011000010101111110010000010011011110111100011010110010100001100110001", "CQ TEST K1ABC/R FN42", &hash_if);
    test_decoding("00001001101111011110001101011000011000010100100111011100000010000101011001001", "K1ABC/R W9XYZ EN37", &hash_if);
    test_decoding("00001100001010010011101110000000010011011110111100011010111010100001100110001", "W9XYZ K1ABC/R R FN42", &hash_if);
    test_decoding("00001001101111011110001101011000011000010100100111011100000111111001110101001", "K1ABC/R W9XYZ RR73", &hash_if);
    test_decoding("00000000011000010101111110010000010011011110111100011010100010100001100110001", "CQ TEST K1ABC FN42", &hash_if);

    test_decoding("01010110101100000000000110100011101000110001000111001010101000000000010001100", "CQ PJ4/K1ABC", &hash_if);
    test_decoding("00001100001010010011101110000000000110101001010110000101000111111010101000001", "W9XYZ <PJ4/K1ABC> -11", &hash_if);
    test_decoding("00000011010100101011000010100000011000010100100111011100001111111010101010001", "<PJ4/K1ABC> W9XYZ R-09", &hash_if);
    test_decoding("00000000000000000000000000100000011000010100100111011100000010000101011001001", "CQ W9XYZ EN37", &hash_if);

    test_decoding("00101111000100000000000000001110111011100011100111111010101100001001110001100", "CQ YW18FIFA", &hash_if);
    test_decoding("00000010101101000010101011000000011000010100100111011100000111111010101000001", "<YW18FIFA> W9XYZ -11", &hash_if);
    test_decoding("00001100001010010011101110000000000101011010000101010110001111111010101010001", "W9XYZ <YW18FIFA> R-09", &hash_if);
    test_decoding("00000010101101000010101011000100101011100011001010010000100111111010010001001", "<YW18FIFA> KA1ABC", &hash_if);
    test_decoding("10010101110001100101001000010000000101011010000101010110000111111010101000001", "KA1ABC <YW18FIFA> -11", &hash_if);
    test_decoding("00000010101101000010101011000100101011100011001010010000101111111010100010001", "<YW18FIFA> KA1ABC R-17", &hash_if);
    test_decoding("00000010101101000010101011000100101011100011001010010000100111111010010100001", "<YW18FIFA> KA1ABC 73", &hash_if);
    test_decoding("00000000000000000000000000100000010010000110000010110011010011111000010011010", "CQ G4ABC/P IO91", &hash_if);
    test_decoding("00001001000011000001011001101101101111011101011000101010000100010011010110010", "G4ABC/P PA9XYZ JO22", &hash_if);
    test_decoding("10110111101110101100010101000000010010000110000010110011010111111001110101010", "PA9XYZ G4ABC/P RR73", &hash_if);
    test_decoding("00110010011000000000000000001000111100000110100011001110110000001001000001100", "CQ KH1/KH7Z", &hash_if);
    test_decoding("11110011000100000000000110100011101000110001000111001010101000000000011000100", "PJ4/K1ABC <W9XYZ>", &hash_if);
    test_decoding("11110011000100000000000110100011101000110001000111001010101000000000010010100", "<W9XYZ> PJ4/K1ABC RRR", &hash_if);
    test_decoding("11110011000100000000000110100011101000110001000111001010101000000000011110100", "PJ4/K1ABC <W9XYZ> 73", &hash_if);
    test_decoding("11110011000100000000000000001110111011100011100111111010101100001001110000100", "<W9XYZ> YW18FIFA", &hash_if);
    test_decoding("11110011000100000000000000001110111011100011100111111010101100001001111010100", "YW18FIFA <W9XYZ> RRR", &hash_if);
    test_decoding("11110011000100000000000000001110111011100011100111111010101100001001110110100", "<W9XYZ> YW18FIFA 73", &hash_if);
    test_decoding("00101101001100000000000000001110111011100011100111111010101100001001110100100", "<KA1ABC> YW18FIFA RR73", &hash_if);
    // Not supported yet
    // test_decoding("00001001101111011110001101010000110000101001001110111000001100100101011001000", "K1ABC RR73; W9XYZ <KH1/KH7Z> -08", &hash_if);
    // test_decoding("00100100011010001010110011110001001101010111100110111101111000000010010101000", "123456789ABCDEF012", &hash_if);
    // test_decoding("00000100110111101111000110101000011000010100100111011100001011111101110001011", "K1ABC W9XYZ 579 WI", &hash_if);
    // test_decoding("00000110000101001001110111000000010011011110111100011010111101111101010101011", "W9XYZ K1ABC R 589 MA", &hash_if);
    // test_decoding("00000100110111101111000110101100101011100001000010001110100111111101011001011", "K1ABC KA0DEF 559 MO", &hash_if);
    // test_decoding("11001010111000010000100011101000010011011110111100011010111001111101010101011", "TU; KA0DEF K1ABC R 569 MA", &hash_if);
    // test_decoding("01001010111000110010100100001000010010000011101000110011000000000000001101011", "KA1ABC G3AAA 529 0013", &hash_if);
    // test_decoding("10000100100000111010001100110000010011011110111100011010110111111101010101011", "TU; G3AAA K1ABC R 559 MA", &hash_if);

    test_msg("YW18FIFA W9XYZ -11", "<YW18FIFA> W9XYZ -11", &hash_if);

    return 0;
}
