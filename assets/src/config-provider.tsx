import React, { createContext, useContext, useState, useEffect, useCallback } from 'react';
import { produce } from 'immer';
import { Millennium } from '@steambrew/client';
import { settingsManager } from './settings-manager';
import { PyGetBackendConfig, PySetBackendConfig } from './utils/ffi';
import { AppConfig } from './AppConfig';

type ConfigContextType = {
	config: AppConfig | null;
	updateConfig: (recipe: (draft: AppConfig) => void) => void;
};

const handler = new EventTarget();
const ConfigContext = createContext<ConfigContextType | null>(null);

const OnBackendConfigUpdate = (config: string) => {
	const parsedConfig = JSON.parse(config) as AppConfig;
	handler.dispatchEvent(new CustomEvent('configUpdated', { detail: parsedConfig }));
};

/** Expose the function to allow it to be callable from the backend */
Millennium.exposeObj({ OnBackendConfigUpdate });

const GetBackendConfig = async (): Promise<AppConfig> => JSON.parse(await PyGetBackendConfig()) as AppConfig;
const SetBackendConfig = async (config: AppConfig): Promise<void> => config && (await PySetBackendConfig({ config: JSON.stringify(config), skip_propagation: true }));

export const ConfigProvider = ({ children }: { children: React.ReactNode }) => {
	const [config, setConfig] = useState<AppConfig>(settingsManager.getConfig());
	const [isLocalUpdate, setIsLocalUpdate] = useState<boolean>(false);

	const OnConfigChange = (event: CustomEvent<AppConfig>) => {
		setIsLocalUpdate(false); // Reset the flag when receiving backend updates
		setConfig(event.detail);
		settingsManager.setConfigDirect(event.detail);
	};

	useEffect(() => {
		handler.addEventListener('configUpdated', OnConfigChange as EventListener);

		GetBackendConfig().then((cfg) => {
			setConfig(cfg);
			settingsManager.setConfigDirect(cfg);
		});

		return () => {
			handler.removeEventListener('configUpdated', OnConfigChange as EventListener);
		};
	}, []);

	const updateConfig = useCallback((recipe: (draft: AppConfig) => void) => {
		setIsLocalUpdate(true); // Set the flag when making local updates
		setConfig((prev: AppConfig) => {
			const next = produce(prev!, recipe);
			settingsManager.setConfigDirect(next);
			return next;
		});
	}, []);

	useEffect(() => {
		settingsManager.setUpdateFunction(updateConfig);
	}, [updateConfig]);

	useEffect(() => {
		if (!isLocalUpdate) {
			return; // Don't propagate if this wasn't a local update
		}
		SetBackendConfig(config);
	}, [config, isLocalUpdate]);

	return <ConfigContext.Provider value={{ config, updateConfig }}>{children}</ConfigContext.Provider>;
};
export const useMillenniumState = () => {
	const ctx = useContext(ConfigContext);
	if (!ctx) throw new Error('useConfig must be used within a ConfigProvider');
	return ctx.config;
};

export const useUpdateConfig = () => {
	const ctx = useContext(ConfigContext);
	if (!ctx) throw new Error('useUpdateConfig must be used within a ConfigProvider');
	return ctx.updateConfig;
};
