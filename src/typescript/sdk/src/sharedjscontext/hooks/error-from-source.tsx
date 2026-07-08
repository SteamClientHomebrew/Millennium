import { ErrorInfo } from 'react';

const pluginErrorRegex = /millennium\.ftp\/.+\/plugins\/([^/]+)\/.millennium\/Dist\/index\.js/;
const millenniumFrontendRegex = /millennium\.ftp\/[a-zA-Z0-9]{32}\/millennium-frontend\.js/;
const millenniumApiRegex = /millennium\.ftp\/[a-zA-Z0-9]{32}\/millennium\.js/;

export interface ValveReactErrorInfo {
	error: Error;
	info: ErrorInfo;
}

export interface ValveError {
	identifier: string;
	identifierHash: string;
	message: string | [func: string, src: string, line: number, column: number];
}

export type ErrorSource = [source: string, wasPlugin: boolean, shouldReportToValve: boolean];

export function getLikelyErrorSourceFromValveError(error: ValveError): ErrorSource {
	return getLikelyErrorSource(JSON.stringify(error?.message));
}

export function getLikelyErrorSourceFromValveReactError(error: ValveReactErrorInfo): ErrorSource {
	return getLikelyErrorSource(error?.error?.stack + '\n' + error.info.componentStack);
}

export function getLikelyErrorSource(error?: string): ErrorSource {

	const pluginMatch = error?.match(pluginErrorRegex);
	if (pluginMatch) {
		return [decodeURIComponent(pluginMatch[1]), true, false];
	}

	if (error && millenniumFrontendRegex.test(error)) {
		return ['the Millennium frontend', false, false];
	}

	if (error && millenniumApiRegex.test(error)) {
		return ['the Millennium API', false, false];
	}

	return ['Steam', false, true];
}
