import { API_URL } from './globals';
import { Logger } from './Logger';

export async function IncrementThemeDownloadFromId(id: string) {
	try {
		const response = await fetch(`${API_URL}/api/bump/theme/${id}`);

		if (!response.ok) {
			Logger.Error('Failed to bump theme downloads');
			return;
		}

		const responseJson = await response.json();

		if (!responseJson?.success) {
			Logger.Error('Failed to bump theme downloads:', responseJson?.message || 'Unknown error');
			return;
		} else {
			Logger.Log('Theme downloads bumped successfully');
		}
	} catch (error) {
		Logger.Error('Error bumping theme downloads:', error);
	}
}

export async function IncrementPluginDownloadFromId(id: string) {
	try {
		const response = await fetch(`${API_URL}/api/bump/plugin/${id}`);

		if (!response.ok) {
			Logger.Error('Failed to bump plugin downloads');
			return;
		}

		const responseJson = await response.json();

		if (!responseJson?.success) {
			Logger.Error('Failed to bump plugin downloads:', responseJson?.message || 'Unknown error');
			return;
		} else {
			Logger.Log('Plugin downloads bumped successfully');
		}
	} catch (error) {
		Logger.Error('Error bumping plugin downloads:', error);
	}
}
