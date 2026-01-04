import { ChromeDevToolsProtocol } from '@steambrew/client';

export class IsolatedContextManager {
	private attachedTargets = new Map<string, number>();
	private contextName = 'MillenniumIsolatedContext';

	constructor() {
		this.initialize();
	}

	private async initialize() {
		await ChromeDevToolsProtocol.send('Target.setDiscoverTargets', { discover: true });

		await this.attachToExistingTargets();
		this.setupEventListeners();
	}

	private async attachToExistingTargets() {
		const { targetInfos } = await ChromeDevToolsProtocol.send('Target.getTargets');
		const webKitTargets = targetInfos.filter(
			(e) =>
				!e?.canAccessOpener &&
				e?.url !== '' &&
				(e?.url?.startsWith('http://') || e?.url?.startsWith('https://')) &&
				!e?.url?.startsWith('https://steamloopback.host/index.html'),
		);

		for (const target of webKitTargets) {
			await this.attachToTarget(target.targetId);
		}
	}

	/**
	 *
	 */
	private async getPluginWebkits() {}

	private async attachToTarget(targetId: string) {
		try {
			const { sessionId } = await ChromeDevToolsProtocol.send('Target.attachToTarget', {
				targetId,
				flatten: true,
			});

			await ChromeDevToolsProtocol.send('Runtime.enable', undefined, sessionId);
			await ChromeDevToolsProtocol.send('Page.enable', undefined, sessionId);

			const { frameTree } = await ChromeDevToolsProtocol.send('Page.getFrameTree', undefined, sessionId);
			const frameId = frameTree.frame.id;

			const { executionContextId } = await ChromeDevToolsProtocol.send(
				'Page.createIsolatedWorld',
				{
					frameId: frameId,
					worldName: this.contextName,
					grantUniveralAccess: true,
				},
				sessionId,
			);

			this.attachedTargets.set(targetId, executionContextId);
			await this.exposeCDPToContext(executionContextId, sessionId);

			console.log(`Attached isolated context to target ${targetId}`);
		} catch (error) {
			console.error(`Failed to attach to target ${targetId}:`, error);
		}
	}

	private async exposeCDPToContext(contextId: number, sessionId: string) {
		const bindingName = 'cdp';

		await ChromeDevToolsProtocol.send(
			'Runtime.addBinding',
			{
				name: bindingName,
				executionContextId: contextId,
			},
			sessionId,
		);

		await ChromeDevToolsProtocol.send(
			'Runtime.evaluate',
			{
				contextId: contextId,
				expression: `
				(function() {
					let nextId = 0;
					const pending = new Map();

					window.${bindingName} = {
						send: (method, params = {}) => {
							return new Promise((resolve, reject) => {
								const id = nextId++;
								pending.set(id, { resolve, reject });
								${bindingName}(JSON.stringify({ id, method, params }));
							});
						},
						__handleResponse: (id, result, error) => {
							const p = pending.get(id);
							if (p) {
								pending.delete(id);
								error ? p.reject(new Error(error.message)) : p.resolve(result);
							}
						}
					};
					console.log('[Millennium] Isolated context ready');
				})();
			`,
			},
			sessionId,
		);
	}

	private setupEventListeners() {
		const originalOnMessage = ChromeDevToolsProtocol.devTools.onmessage;

		ChromeDevToolsProtocol.devTools.onmessage = (message: any) => {
			const data = typeof message === 'string' ? JSON.parse(message) : message;

			if (data.method === 'Target.targetCreated') {
				const targetInfo = data.params.targetInfo;
				if (
					!targetInfo.canAccessOpener &&
					targetInfo.url !== '' &&
					(targetInfo.url?.startsWith('http://') || targetInfo.url?.startsWith('https://')) &&
					!targetInfo.url?.startsWith('https://steamloopback.host/index.html')
				) {
					this.attachToTarget(targetInfo.targetId);
				}
			}

			if (data.method === 'Target.targetDestroyed') {
				this.attachedTargets.delete(data.params.targetId);
			}

			if (data.method === 'Target.targetInfoChanged') {
				const targetInfo = data.params.targetInfo;
				if (this.attachedTargets.has(targetInfo.targetId)) {
					this.attachToTarget(targetInfo.targetId);
				}
			}

			if (data.method === 'Runtime.bindingCalled' && data.params?.name === 'cdp') {
				const { id, method, params } = JSON.parse(data.params.payload);
				const sessionId = data.sessionId;

				ChromeDevToolsProtocol.send(method, params, sessionId)
					.then((result) => {
						return ChromeDevToolsProtocol.send(
							'Runtime.evaluate',
							{
								contextId: data.params.executionContextId,
								expression: `cdp.__handleResponse(${id}, ${JSON.stringify(result)}, null)`,
							},
							sessionId,
						);
					})
					.catch((error) => {
						return ChromeDevToolsProtocol.send(
							'Runtime.evaluate',
							{
								contextId: data.params.executionContextId,
								expression: `cdp.__handleResponse(${id}, null, ${JSON.stringify({ message: error.message })})`,
							},
							sessionId,
						);
					});
			} else {
				originalOnMessage(message);
			}
		};
	}
}
