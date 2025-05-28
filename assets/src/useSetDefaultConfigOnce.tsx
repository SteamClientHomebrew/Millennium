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
