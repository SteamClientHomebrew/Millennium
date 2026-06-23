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

import {
    ffi as realFfi,
    callable as realCallable,
    Millennium as realMillennium,
    BindPluginSettings as realBindPluginSettings,
    subscribePluginConfig as realSubscribePluginConfig,
    pluginSelf as realPluginSelf
} from '@steambrew/sdk/src/millennium-api';

let TEST_PLUGIN_NAME = 'core';

export function setTestPluginName(name: string): void {
	TEST_PLUGIN_NAME = name;
}

export function getTestPluginName(): string {
	return TEST_PLUGIN_NAME;
}

export const ffi: <Args extends unknown[] = [], Ret = unknown>(route: string) => (...args: Args) => Promise<Ret> =
	(route: string) => realFfi(TEST_PLUGIN_NAME, route) as any;

export const callable: <Args extends [Record<string, unknown>] | [] = [], Ret = unknown>(route: string) => (...args: Args) => Promise<Ret> =
	(route: string) => realCallable(TEST_PLUGIN_NAME, route) as any;

export const Millennium = {
	callServerMethod: (methodName: string, kwargs?: object) => realMillennium.callServerMethod(TEST_PLUGIN_NAME, methodName, kwargs),
	findElement: realMillennium.findElement,
	exposeObj: realMillennium.exposeObj,
};

export const BindPluginSettings = () => realBindPluginSettings(TEST_PLUGIN_NAME);

export const subscribePluginConfig = (cb: (key: string, value: any) => void): (() => void) =>
	realSubscribePluginConfig(TEST_PLUGIN_NAME, cb);

export const pluginSelf = realPluginSelf;
