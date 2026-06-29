import { Logger } from './logger';

const logger = new Logger('ErrorMonitor');

declare const __SDK_VERSION__: string;
const SDK_VERSION = typeof __SDK_VERSION__ !== 'undefined' ? __SDK_VERSION__ : 'unknown';

function report(message: string, source?: string, error?: unknown): never {
	logger.error(
		`\n\n  SDK version ${SDK_VERSION} may be incompatible with the current Steam build.\n` +
			`  Please report this at https://github.com/SteamClientHomebrew/Millennium/issues\n\n` +
			`  ${message}` +
			(source ? `\n  Source: ${source}` : '') +
			(error instanceof Error && error.stack ? `\n\n${error.stack}` : ''),
	);
	if (error instanceof Error) throw error;
	throw new Error(message);
}

export function guardedInit<T>(fn: () => T, label: string): T | undefined {
	try {
		return fn();
	} catch (err) {
		report(`SDK failed to initialize: ${label}`, undefined, err);
	}
}
