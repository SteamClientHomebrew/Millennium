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

import { useRef } from 'react';
import { useMillenniumState, useUpdateConfig } from './config-provider';
import { AppConfig } from './AppConfig';
// Utility: deep merge only when keys don't exist
function mergeIfMissing<T>(target: T, source: Partial<T>): T {
	const output: any = { ...target };

	for (const key in source) {
		const sourceValue = source[key];
		const targetValue = target?.[key];

		if (typeof sourceValue === 'object' && sourceValue !== null && !Array.isArray(sourceValue)) {
			output[key] = mergeIfMissing(targetValue && typeof targetValue === 'object' ? targetValue : {}, sourceValue);
		} else if (targetValue === undefined) {
			output[key] = sourceValue;
		} else {
			output[key] = targetValue;
		}
	}

	return output;
}

export const useSetDefaultConfigOnce = () => {
	const config = useMillenniumState();
	const setConfig = useUpdateConfig();
	const didSetRef = useRef(false);

	return (defaults: Partial<AppConfig>) => {
		if (didSetRef.current) {
			console.warn('Default config has already been set once. Skipping.');
			return;
		}

		console.log('Setting default config:', defaults);
		setConfig((prev) => mergeIfMissing(prev, defaults));

		didSetRef.current = true;
	};
};
