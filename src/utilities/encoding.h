#pragma once
#include <string>
#include <vector>

static const std::string base64_chars =
             "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
             "abcdefghijklmnopqrstuvwxyz"
             "0123456789+/";
/**
 * @brief Checks if a character is a valid base64 character.
 *
 * @param c The character to be checked.
 *
 * @return true if the character is a valid base64 character, false otherwise.
 *
 * @remarks
 * - A character is considered a valid base64 character if it is alphanumeric or one of the following: '+', '/'.
 */
static inline bool is_base64(unsigned char c) {
  return (isalnum(c) || (c == '+') || (c == '/'));
}

/**
 * @brief Decodes a base64 encoded string.
 *
 * @param input The base64 encoded input string.
 *
 * @return std::string The decoded string.
 *
 * @remarks
 * - Decodes the input string into its original form.
 * - Removes padding characters ('=') if present.
 */
static std::string base64_decode(const std::string &in) {

    std::string out;

    std::vector<int> T(256,-1);
    for (int i=0; i<64; i++) T["ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[i]] = i;

    int val=0, valb=-8;
    for (unsigned char c : in) {
        if (T[c] == -1) break;
        val = (val << 6) + T[c];
        valb += 6;
        if (valb >= 0) {
            out.push_back(char((val>>valb)&0xFF));
            valb -= 8;
        }
    }
    return out;
}

/**
 * @brief Encodes a string into base64 format.
 *
 * @param input The input string to be encoded.
 *
 * @return std::string The base64 encoded string.
 *
 * @remarks
 * - Encodes the input string into base64 format.
 * - Adds padding characters ('=') if necessary to make the length a multiple of 4.
 */
static std::string Base64Encode(const std::string &in) {
    std::string out;

    int val = 0, valb = -6;
    for (unsigned char c : in) {
        val = (val << 8) + c;
        valb += 8;
        while (valb >= 0) {
            out.push_back("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[(val>>valb)&0x3F]);
            valb -= 6;
        }
    }
    if (valb>-6) out.push_back("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[((val<<8)>>(valb+8))&0x3F]);
    while (out.size()%4) out.push_back('=');
    return out;
}