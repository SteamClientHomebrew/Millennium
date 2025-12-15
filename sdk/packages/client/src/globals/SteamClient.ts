import { SteamClient } from './steam-client';
export * from './steam-client';

declare global {
	var SteamClient: SteamClient;
}
