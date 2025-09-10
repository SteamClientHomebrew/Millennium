/**
 * ==================================================
 *   _____ _ _ _             _
 *  |     |_| | |___ ___ ___|_|_ _ _____
 *  | | | | | | | -_|   |   | | | |     |
 *  |_|_|_|_|_|_|___|_|_|_|_|_|___|_|_|_|
 *
 * ==================================================
 *
 * Copyright (c) 2025 Project Millennium
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

#include "millennium/auth.h"
#include "millennium/logger.h"
#include <random>

/**
 * @brief Build a random string of given length.
 * This function generates a random string of the specified length using
 * alphanumeric characters.
 *
 * It uses mersenne twister engine for random number generation.
 */
std::string BuildRandomString(size_t length)
{
    const std::string charset = "0123456789"
                                "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                "abcdefghijklmnopqrstuvwxyz";

    std::random_device rd;        // Non-deterministic random number generator
    std::mt19937 generator(rd()); // Mersenne Twister engine
    std::uniform_int_distribution<> dist(0, charset.size() - 1);

    std::string token;
    token.reserve(length);

    for (size_t i = 0; i < length; ++i) {
        token += charset[dist(generator)];
    }

    return token;
}

/**
 * Just a basic security measure, and doesn't overly matter since this is all local.
 * But it stops casual snooping. It also prevent actual fs paths clashing with internal api paths.
 */
const std::string GetScrambledApiPathToken()
{
    static const std::string scrambledApiPathToken = BuildRandomString(32);
    return scrambledApiPathToken;
}

/**
 * Get the auth token for the current session.
 * Currently only used in the IPC to validate requests, but it can be used elsewhere if needed.
 *
 * @returns {std::string} - The auth token for the current session.
 */
const std::string GetAuthToken()
{
    static const std::string authToken = BuildRandomString(128);
    return authToken;
}
