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

import type { BackendShape, BackendFixtures, ContractReport, EndpointReport, EndpointFixture, FFICall } from './types';
import type { FFIStub } from './ffi-stub';

const VALID_IPC_METHODS = new Set([0, 1, 2, 3]); // CALL_SERVER_METHOD, FRONT_END_LOADED, CALL_FRONTEND_METHOD, PLUGIN_CONFIG

function isFFICall(value: unknown): value is FFICall {
	return typeof value === 'function';
}

function isFixtureLeaf(value: unknown): value is EndpointFixture<unknown[], unknown> {
	return typeof value === 'object' && value !== null && 'route' in value && 'args' in value && 'response' in value;
}

function deepEqual(a: unknown, b: unknown): boolean {
	if (a === b) return true;
	if (typeof a !== typeof b) return false;
	if (a === null || b === null) return false;
	if (typeof a !== 'object') return false;
	if (Array.isArray(a) !== Array.isArray(b)) return false;
	if (Array.isArray(a) && Array.isArray(b)) {
		if (a.length !== b.length) return false;
		return a.every((v, i) => deepEqual(v, b[i]));
	}
	const ka = Object.keys(a as object);
	const kb = Object.keys(b as object);
	if (ka.length !== kb.length) return false;
	return ka.every((k) => deepEqual((a as any)[k], (b as any)[k]));
}

async function exerciseEndpoint(
	fn: FFICall,
	fixture: EndpointFixture<unknown[], unknown>,
	path: string[],
	stub: FFIStub,
): Promise<EndpointReport> {
	const failures: string[] = [];

	stub.reset();
	const successPromise = fn(...fixture.args);

	const captured = stub.lastCall;
	if (!captured) {
		failures.push('No outbound binding call was captured — did the SDK silently drop the call?');
	} else {
		if (!VALID_IPC_METHODS.has(captured.id)) {
			failures.push(`Outbound payload.id = ${captured.id}, expected one of {0,1,2,3} (IpcMethod ordinals).`);
		}
		if (captured.data.methodName !== fixture.route) {
			failures.push(`Wire route mismatch: payload.data.methodName = ${JSON.stringify(captured.data.methodName)}, fixture.route = ${JSON.stringify(fixture.route)}.`);
		}
		if (!deepEqual(captured.data.argumentList, fixture.args)) {
			failures.push(`Wire args mismatch: payload.data.argumentList = ${JSON.stringify(captured.data.argumentList)}, fixture.args = ${JSON.stringify(fixture.args)}.`);
		}
		if (typeof captured.call_id !== 'number' || !Number.isInteger(captured.call_id)) {
			failures.push(`payload.call_id is not an integer: ${JSON.stringify(captured.call_id)}.`);
		}
		if (typeof captured.data.pluginName !== 'string' || captured.data.pluginName.length === 0) {
			failures.push(`payload.data.pluginName is missing or empty: ${JSON.stringify(captured.data.pluginName)}.`);
		}

		stub.respond(captured.call_id, fixture.response);
		try {
			const resolved = await successPromise;
			const isVoidEquivalent = fixture.response === undefined && resolved === null;
			if (!isVoidEquivalent && !deepEqual(resolved, fixture.response)) {
				failures.push(`Resolved value did not match fixture.response. Got ${JSON.stringify(resolved)}, expected ${JSON.stringify(fixture.response)}.`);
			}
		} catch (e) {
			failures.push(`Promise rejected on success path: ${(e as Error).message}`);
		}
	}

	if (fixture.errorResponse !== undefined) {
		stub.reset();
		const errorPromise = fn(...fixture.args);
		const errCaptured = stub.lastCall;
		if (errCaptured) {
			stub.fail(errCaptured.call_id, fixture.errorResponse);
			try {
				await errorPromise;
				failures.push('Error path: Promise resolved instead of rejecting.');
			} catch (e) {
				const msg = (e as Error).message;
				if (!msg.includes(fixture.errorResponse)) {
					failures.push(`Error path: rejection message ${JSON.stringify(msg)} did not contain ${JSON.stringify(fixture.errorResponse)}.`);
				}
			}
		}
	}

	return { path, route: fixture.route, pass: failures.length === 0, failures };
}

async function walk(
	backend: BackendShape,
	fixtures: Record<string, unknown>,
	prefix: string[],
	stub: FFIStub,
	out: EndpointReport[],
): Promise<void> {
	for (const key of Object.keys(backend)) {
		const node = backend[key];
		const fixture = fixtures[key];
		const path = [...prefix, key];

		if (isFFICall(node)) {
			if (!isFixtureLeaf(fixture)) {
				out.push({
					path,
					route: '<unknown>',
					pass: false,
					failures: [`Missing fixture for endpoint at ${path.join('.')}`],
				});
				continue;
			}
			out.push(await exerciseEndpoint(node, fixture, path, stub));
		} else if (typeof node === 'object' && node !== null) {
			if (typeof fixture !== 'object' || fixture === null || isFixtureLeaf(fixture)) {
				out.push({
					path,
					route: '<unknown>',
					pass: false,
					failures: [`Expected nested fixture object at ${path.join('.')}, got ${typeof fixture}.`],
				});
				continue;
			}
			await walk(node as BackendShape, fixture as Record<string, unknown>, path, stub, out);
		}
	}
}

function findOrphans(backend: BackendShape, fixtures: Record<string, unknown>, prefix: string[], out: EndpointReport[]): void {
	for (const key of Object.keys(fixtures)) {
		const fixture = fixtures[key];
		const node = backend[key];
		const path = [...prefix, key];

		if (isFixtureLeaf(fixture)) {
			if (!isFFICall(node)) {
				out.push({
					path,
					route: fixture.route,
					pass: false,
					failures: [`Orphan fixture at ${path.join('.')} — no matching endpoint in backend. Remove the fixture or restore the endpoint.`],
				});
			}
		} else if (typeof fixture === 'object' && fixture !== null) {
			if (typeof node !== 'object' || node === null || isFFICall(node)) {
				out.push({
					path,
					route: '<unknown>',
					pass: false,
					failures: [`Orphan fixture namespace at ${path.join('.')} — no matching object in backend.`],
				});
				continue;
			}
			findOrphans(node as BackendShape, fixture as Record<string, unknown>, path, out);
		}
	}
}

export async function runContractSuite<T extends BackendShape>(backend: T, fixtures: BackendFixtures<T>, stub: FFIStub): Promise<ContractReport> {
	const endpoints: EndpointReport[] = [];
	const fixturesAny = fixtures as unknown as Record<string, unknown>;
	await walk(backend, fixturesAny, [], stub, endpoints);
	findOrphans(backend, fixturesAny, [], endpoints);

	return {
		endpoints,
		get passed() {
			return endpoints.filter((e) => e.pass).length;
		},
		get failed() {
			return endpoints.filter((e) => !e.pass).length;
		},
	};
}

export function formatContractReport(report: ContractReport): string {
	const lines: string[] = [];
	lines.push(`Contract: ${report.passed} passed, ${report.failed} failed (${report.endpoints.length} total)`);
	for (const ep of report.endpoints) {
		const icon = ep.pass ? '  PASS' : '  FAIL';
		lines.push(`${icon}  ${ep.path.join('.')}  →  ${ep.route}`);
		for (const failure of ep.failures) {
			lines.push(`        - ${failure}`);
		}
	}
	return lines.join('\n');
}
