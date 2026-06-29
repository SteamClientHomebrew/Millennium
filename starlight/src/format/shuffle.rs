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
fn lcg(state: u32) -> u32 {
    state.wrapping_mul(1664525).wrapping_add(1013904223)
}

#[allow(dead_code)]
pub fn encode(data: &mut [u8], seed: u32) {
    let mut state = seed;
    for i in (1..data.len()).rev() {
        state = lcg(state);
        let j = (state as usize) % (i + 1);
        data.swap(i, j);
    }

    let mut xstate = seed.wrapping_add(0x9E3779B9);
    for byte in data.iter_mut() {
        xstate = lcg(xstate);
        *byte ^= (xstate >> 24) as u8;
    }
}

#[allow(dead_code)]
pub fn decode(data: &mut [u8], seed: u32) {
    let mut xstate = seed.wrapping_add(0x9E3779B9);
    for byte in data.iter_mut() {
        xstate = lcg(xstate);
        *byte ^= (xstate >> 24) as u8;
    }

    let n = data.len();
    let mut state = seed;
    let mut swaps: Vec<(usize, usize)> = Vec::with_capacity(n);
    for i in (1..n).rev() {
        state = lcg(state);
        let j = (state as usize) % (i + 1);
        swaps.push((i, j));
    }
    for &(i, j) in swaps.iter().rev() {
        data.swap(i, j);
    }
}
