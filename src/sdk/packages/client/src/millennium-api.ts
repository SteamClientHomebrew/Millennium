import ProtocolMapping from 'devtools-protocol/types/protocol-mapping';

/** Returnable IPC types */
type IPCType = string | number | boolean | void;

/*
 Global Millennium API for developers.
*/
type Millennium = {
	/**
	 * Call a method on the backend
	 * @deprecated Use `callable` instead.
	 * Example usage:
	 * ```typescript
	 * // before
	 * const result = await Millennium.callServerMethod('methodName', { arg1: 'value' });
	 * // after
	 * const method = callable<[{ arg1: string }]>("methodName");
	 *
	 * const result1 = await method({ arg1: 'value1' });
	 * const result2 = await method({ arg1: 'value2' });
	 * ```
	 */
	callServerMethod: (methodName: string, kwargs?: object) => Promise<any>;
	findElement: (privateDocument: Document, querySelector: string, timeOut?: number) => Promise<NodeListOf<Element>>;
	exposeObj?: <T extends object>(obj: T) => any;
	AddWindowCreateHook?: (callback: (context: object) => void) => void;
};

/**
 * Make reusable IPC call declarations
 *
 * frontend:
 * ```typescript
 * const method = callable<[{ arg1: string }]>("methodName"); // declare the method
 * method({ arg1: 'value' }); // call the method
 * ```
 *
 * backend:
 * ```python
 * def methodName(arg1: str):
 *    pass
 * ```
 */
declare const callable: <
	// Ideally this would be `Params extends Record<...>` but for backwards compatibility we keep a tuple type
	Params extends [params: Record<string, IPCType>] | [] = [],
	Return extends IPCType = IPCType
>(route: string) => (...params: Params) => Promise<Return>;

const m_private_context: any = undefined;
export const pluginSelf = m_private_context;

declare global {
	interface Window {
		Millennium: Millennium;
		MILLENNIUM_CHROME_DEV_TOOLS_PROTOCOL_DO_NOT_USE_OR_OVERRIDE_ONMESSAGE: any;
		CHROME_DEV_TOOLS_PROTOCOL_API_BINDING: MillenniumChromeDevToolsProtocol;
	}
}

declare const BindPluginSettings: () => any;

interface CDPMessage {
	id?: number;
	method: string;
	params?: Record<string, any>;
	sessionId?: string; // Optional session ID for targeted commands
	result?: any;
	error?: {
		message: string;
		code: number;
	};
}

class MillenniumChromeDevToolsProtocol {
	devTools: any;
	currentId: number;
	pendingRequests: Map<number, { resolve: (value: any) => void; reject: (reason?: any) => void }>;

	constructor() {
		this.devTools = window.MILLENNIUM_CHROME_DEV_TOOLS_PROTOCOL_DO_NOT_USE_OR_OVERRIDE_ONMESSAGE;
		this.currentId = 0;
		this.pendingRequests = new Map();

		// Override the onmessage callback to handle responses
		this.devTools.onmessage = (message: any) => {
			this.handleMessage(message);
		};
	}

	handleMessage(message: any) {
		const data = typeof message === 'string' ? JSON.parse(message) : message;

		// Check if this is a response to one of our requests
		if (data.id !== undefined && this.pendingRequests.has(data.id)) {
			const pending = this.pendingRequests.get(data.id);
			this.pendingRequests.delete(data.id);

			if (pending) {
				const { resolve, reject } = pending;
				if (data.error) {
					reject(new Error(`CDP Error: ${data.error.message} (${data.error.code})`));
				} else {
					resolve(data.result);
				}
			}
		}
	}

	send<T extends keyof ProtocolMapping.Commands>(method: T, params: ProtocolMapping.Commands[T]['paramsType'][0] = {}, sessionId?: string): Promise<ProtocolMapping.Commands[T]['returnType']> {
		return new Promise((resolve, reject) => {
			const id = this.currentId++;

			// Store the promise resolvers
			this.pendingRequests.set(id, { resolve, reject });

			// Prepare the message
			const message: CDPMessage = {
				id: id,
				method: method,
			};

			// Only add params if they're provided and not empty
			if (params && Object.keys(params).length > 0) {
				message.params = params;
			}

			// If a sessionId is provided, include it in the message
			if (sessionId) {
				message.sessionId = sessionId;
			}

			// Send the message
			try {
				this.devTools.send(JSON.stringify(message));
			} catch (error) {
				// Clean up if send fails
				this.pendingRequests.delete(id);
				reject(error);
			}
		});
	}

	// Helper method to send without waiting for response (fire and forget)
	sendNoResponse<T extends keyof ProtocolMapping.Commands>(method: T, params: ProtocolMapping.Commands[T]['paramsType'][0] = {}) {
		const message: CDPMessage = {
			id: this.currentId++,
			method: method,
		};

		if (params && Object.keys(params).length > 0) {
			message.params = params;
		}

		this.devTools.send(JSON.stringify(message));
	}
}

const ChromeDevToolsProtocol: MillenniumChromeDevToolsProtocol = new MillenniumChromeDevToolsProtocol();
const Millennium: Millennium = window.Millennium;
export { BindPluginSettings, callable, ChromeDevToolsProtocol, Millennium };

