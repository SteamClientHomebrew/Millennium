import { afterPatch, findInReactTree, getReactRoot } from '@steambrew/client';
import { RenderSettingsModal } from '../settings';

export function PatchRootMenu() {
	const steamRootMenu = findInReactTree(getReactRoot(document.getElementById('root') as any), (m) => {
		return m?.pendingProps?.title === 'Steam' && m?.pendingProps?.menuContent;
	});

	afterPatch(steamRootMenu.pendingProps.menuContent, 'type', RenderSettingsModal);
}
