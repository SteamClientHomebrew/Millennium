/**
 * ==================================================
 *   _____ _ _ _             _
 *  |     |_| | |___ ___ ___|_|_ _ _____
 *  | | | | | | | -_|   |   | | | |     |
 *  |_|_|_|_|_|_|___|_|_|_|_|_|___|_|_|_|
 *
 * ==================================================
 *
 * Copyright (c) 2026 Project Millennium
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "millennium/crypto.h"

#include <cstdint>
#include <string>
#include <vector>

static constexpr std::string_view BLOB_PREFIX = "MGCRYPT:v1:";

static const char kB64Chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static const int8_t kB64Inv[256] = {
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,62,-1,-1,-1,63,
    52,53,54,55,56,57,58,59,60,61,-1,-1,-1,-1,-1,-1,
    -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,
    15,16,17,18,19,20,21,22,23,24,25,-1,-1,-1,-1,-1,
    -1,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,
    41,42,43,44,45,46,47,48,49,50,51,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
};

static std::string base64_encode(const uint8_t* data, size_t size)
{
    std::string out;
    out.reserve(((size + 2) / 3) * 4);
    for (size_t i = 0; i < size; i += 3)
    {
        uint32_t n = static_cast<uint32_t>(data[i]) << 16;
        if (i + 1 < size) n |= static_cast<uint32_t>(data[i + 1]) << 8;
        if (i + 2 < size) n |= static_cast<uint32_t>(data[i + 2]);
        out += kB64Chars[(n >> 18) & 0x3F];
        out += kB64Chars[(n >> 12) & 0x3F];
        out += (i + 1 < size) ? kB64Chars[(n >> 6) & 0x3F] : '=';
        out += (i + 2 < size) ? kB64Chars[n & 0x3F] : '=';
    }
    return out;
}

static std::vector<uint8_t> base64_decode(const std::string& s)
{
    std::vector<uint8_t> out;
    out.reserve(s.size() * 3 / 4);
    uint32_t bits = 0;
    int bit_count = 0;
    for (unsigned char c : s)
    {
        if (c == '=') break;
        int8_t v = kB64Inv[c];
        if (v < 0) continue;
        bits = (bits << 6) | static_cast<uint32_t>(v);
        bit_count += 6;
        if (bit_count >= 8)
        {
            bit_count -= 8;
            out.push_back(static_cast<uint8_t>((bits >> bit_count) & 0xFF));
        }
    }
    return out;
}

#ifdef _WIN32

#include <windows.h>
#include <wincrypt.h>

std::string Crypto::encrypt(const std::string& plaintext)
{
    if (plaintext.empty()) return "";
    DATA_BLOB in_blob  = { static_cast<DWORD>(plaintext.size()), reinterpret_cast<BYTE*>(const_cast<char*>(plaintext.data())) };
    DATA_BLOB out_blob = {};
    if (!CryptProtectData(&in_blob, L"MillenniumProxy", nullptr, nullptr, nullptr, 0, &out_blob))
        return "";
    std::string result = std::string(BLOB_PREFIX) + base64_encode(out_blob.pbData, out_blob.cbData);
    LocalFree(out_blob.pbData);
    return result;
}

std::string Crypto::decrypt(const std::string& ciphertext)
{
    if (!is_encrypted(ciphertext)) return "";
    auto decoded = base64_decode(ciphertext.substr(BLOB_PREFIX.size()));
    if (decoded.empty()) return "";
    DATA_BLOB in_blob  = { static_cast<DWORD>(decoded.size()), decoded.data() };
    DATA_BLOB out_blob = {};
    if (!CryptUnprotectData(&in_blob, nullptr, nullptr, nullptr, nullptr, 0, &out_blob))
        return "";
    std::string result(reinterpret_cast<char*>(out_blob.pbData), out_blob.cbData);
    LocalFree(out_blob.pbData);
    return result;
}

#else

#include <array>
#include <cstring>
#include <fstream>

#include <openssl/evp.h>
#include <openssl/rand.h>

#ifdef __APPLE__
#include <sys/sysctl.h>
#else
#include <unistd.h>
#endif

static std::array<uint8_t, 32> derive_machine_key()
{
    std::string seed;

#ifdef __APPLE__
    char uuid[64] = {};
    size_t len = sizeof(uuid);
    if (sysctlbyname("kern.uuid", uuid, &len, nullptr, 0) == 0)
        seed.assign(uuid, len > 0 && uuid[len - 1] == '\0' ? len - 1 : len);
#else
    std::ifstream f("/etc/machine-id");
    if (f) std::getline(f, seed);
    if (seed.empty())
    {
        char hostname[256] = {};
        gethostname(hostname, sizeof(hostname));
        seed = hostname;
    }
#endif

    seed += "millennium-proxy-secret-v1";

    std::array<uint8_t, 32> key = {};
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    unsigned int out_len = 32;
    EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr);
    EVP_DigestUpdate(ctx, seed.data(), seed.size());
    EVP_DigestFinal_ex(ctx, key.data(), &out_len);
    EVP_MD_CTX_free(ctx);
    return key;
}

std::string Crypto::encrypt(const std::string& plaintext)
{
    if (plaintext.empty()) return "";

    auto key = derive_machine_key();

    std::array<uint8_t, 12> iv = {};
    if (RAND_bytes(iv.data(), 12) != 1) return "";

    std::vector<uint8_t> ciphertext(plaintext.size());
    std::array<uint8_t, 16> tag = {};

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) return "";
    EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr);
    EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, 12, nullptr);
    EVP_EncryptInit_ex(ctx, nullptr, nullptr, key.data(), iv.data());

    int len1 = 0, len2 = 0;
    EVP_EncryptUpdate(ctx, ciphertext.data(), &len1, reinterpret_cast<const uint8_t*>(plaintext.data()), static_cast<int>(plaintext.size()));
    EVP_EncryptFinal_ex(ctx, ciphertext.data() + len1, &len2);
    EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, 16, tag.data());
    EVP_CIPHER_CTX_free(ctx);

    std::vector<uint8_t> packed;
    packed.reserve(12 + 16 + ciphertext.size());
    packed.insert(packed.end(), iv.begin(), iv.end());
    packed.insert(packed.end(), tag.begin(), tag.end());
    packed.insert(packed.end(), ciphertext.begin(), ciphertext.begin() + len1 + len2);

    return std::string(BLOB_PREFIX) + base64_encode(packed.data(), packed.size());
}

std::string Crypto::decrypt(const std::string& ciphertext)
{
    if (!is_encrypted(ciphertext)) return "";

    auto decoded = base64_decode(ciphertext.substr(BLOB_PREFIX.size()));
    if (decoded.size() < 29) return "";

    auto key = derive_machine_key();

    std::array<uint8_t, 12> iv = {};
    std::memcpy(iv.data(), decoded.data(), 12);

    std::array<uint8_t, 16> tag = {};
    std::memcpy(tag.data(), decoded.data() + 12, 16);

    const uint8_t* payload     = decoded.data() + 28;
    const size_t   payload_len = decoded.size() - 28;

    std::vector<uint8_t> plain(payload_len);

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) return "";
    EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr);
    EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, 12, nullptr);
    EVP_DecryptInit_ex(ctx, nullptr, nullptr, key.data(), iv.data());

    int len1 = 0, len2 = 0;
    EVP_DecryptUpdate(ctx, plain.data(), &len1, payload, static_cast<int>(payload_len));
    EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, 16, tag.data());
    int ok = EVP_DecryptFinal_ex(ctx, plain.data() + len1, &len2);
    EVP_CIPHER_CTX_free(ctx);

    if (ok <= 0) return ""; // Authentication tag mismatch

    return std::string(plain.begin(), plain.begin() + len1 + len2);
}

#endif // _WIN32

bool Crypto::is_encrypted(const std::string& value)
{
    return value.size() > BLOB_PREFIX.size() && value.compare(0, BLOB_PREFIX.size(), BLOB_PREFIX) == 0;
}
