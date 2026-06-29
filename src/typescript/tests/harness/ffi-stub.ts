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

import type { CapturedCall } from './types';

export interface FFIStub {
	readonly calls: ReadonlyArray<CapturedCall>;
	readonly lastCall: CapturedCall | undefined;
	respond(callId: number, returnJson: unknown): void;
	fail(callId: number, errorMessage: string): void;
	reset(): void;
	uninstall(): void;
}

const BINDING_NAME = '__private_millennium_ffi_do_not_use__';
const FRONTEND_BINDING = 'MILLENNIUM_PRIVATE_INTERNAL_FOREIGN_FUNCTION_INTERFACE_DO_NOT_USE';

function getBinder(): { __handleResponse(id: number, response: { success: boolean; returnJson: unknown }): void } {
	const ffi = (window as any)[FRONTEND_BINDING];
	if (!ffi || typeof ffi.__handleResponse !== 'function') {
		throw new Error("@steambrew/test: FFI binder not initialized. The loader's millennium-api module must be imported before responding.");
	}
	return ffi;
}

export function installFFIStub(): FFIStub {
	const calls: CapturedCall[] = [];

	const binding = (rawPayload: string) => {
		calls.push(JSON.parse(rawPayload) as CapturedCall);
	};

	(window as unknown as Record<string, unknown>)[BINDING_NAME] = binding;

	return {
		get calls() {
			return calls;
		},
		get lastCall() {
			return calls[calls.length - 1];
		},
		respond(callId, returnJson) {
			getBinder().__handleResponse(callId, { success: true, returnJson });
		},
		fail(callId, errorMessage) {
			getBinder().__handleResponse(callId, { success: false, returnJson: errorMessage });
		},
		reset() {
			calls.length = 0;
		},
		uninstall() {
			delete (window as unknown as Record<string, unknown>)[BINDING_NAME];
			calls.length = 0;
		},
	};
}
