#pragma once
#include <string>
#include <vector>

static std::string base64_chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZ""abcdefghijklmnopqrstuvwxyz""0123456789+/";

bool is_base64(unsigned char c) {
    return (isalnum(c) || (c == '+') || (c == '/'));
}

static std::string base64_decode(const std::string& input) {

    std::vector<unsigned char> decoded_bytes;
    int in_len = input.length();
    int i = 0;

    while (in_len-- && input[i] != '=' && is_base64(input[i])) {
        int byte1 = base64_chars.find(input[i++]);
        int byte2 = base64_chars.find(input[i++]);
        int byte3 = base64_chars.find(input[i++]);
        int byte4 = base64_chars.find(input[i++]);

        int decoded = (byte1 << 18) | (byte2 << 12) | (byte3 << 6) | byte4;
        decoded_bytes.push_back((decoded >> 16) & 0xFF);
        if (input[i - 2] != '=')
            decoded_bytes.push_back((decoded >> 8) & 0xFF);
        if (input[i - 1] != '=')
            decoded_bytes.push_back(decoded & 0xFF);
    }

    return std::string(decoded_bytes.begin(), decoded_bytes.end());
}

static std::string base64_encode(const std::string& input) {

    std::string encoded_string;
    int in_len = input.length();
    int i = 0;

    while (i < in_len) {
        unsigned char byte1 = input[i++];
        unsigned char byte2 = (i < in_len) ? input[i++] : 0;
        unsigned char byte3 = (i < in_len) ? input[i++] : 0;

        unsigned int combined = (byte1 << 16) | (byte2 << 8) | byte3;

        encoded_string += base64_chars[(combined >> 18) & 0x3F];
        encoded_string += base64_chars[(combined >> 12) & 0x3F];
        encoded_string += base64_chars[(combined >> 6) & 0x3F];
        encoded_string += base64_chars[combined & 0x3F];
    }

    size_t padding = 3 - (in_len % 3);
    while (padding > 0 && padding < 3) {
        encoded_string[encoded_string.length() - padding] = '=';
        padding--;
    }

    return encoded_string;
}
