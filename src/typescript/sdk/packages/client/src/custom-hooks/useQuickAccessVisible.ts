import { useEffect, useState } from 'react';

import { getGamepadNavigationTrees } from '../utils';

function getQuickAccessWindow(): Window | null {
	const navTrees = getGamepadNavigationTrees();
	return navTrees.find((tree: any) => tree?.id === 'QuickAccess-NA')?.m_Root?.m_element?.ownerDocument.defaultView ?? null;
}

/**
 * Returns state indicating the visibility of quick access menu.
 *
 * @returns `true` if quick access menu is visible and `false` otherwise.
 *
 * @example
 * import { FC, useEffect } from "react";
 * import { useQuickAccessVisible } from "@steambrew/client";
 *
 * export const PluginPanelView: FC<{}> = ({ }) => {
 *   const isVisible = useQuickAccessVisible();
 *
 *   useEffect(() => {
 *     if (!isVisible) {
 *       return;
 *     }
 *
 *     const interval = setInterval(() => console.log("Hello world!"), 1000);
 *     return () => {
 *       clearInterval(interval);
 *     }
 *   }, [isVisible])
 *
 *   return (
 *     <div>
 *       {isVisible ? "VISIBLE" : "INVISIBLE"}
 *     </div>
 *   );
 * };
 */
export function useQuickAccessVisible(): boolean {
	// By default we say that document is not hidden, unless we know otherwise.
	// This would cover the cases when Valve breaks something and the quick access window
	// cannot be accessed anymore - the plugins that use this would continue working somewhat.
	const [isHidden, setIsHidden] = useState(getQuickAccessWindow()?.document.hidden ?? false);

	useEffect(() => {
		const quickAccessWindow = getQuickAccessWindow();
		if (quickAccessWindow === null) {
			console.error('Could not get window of QuickAccess menu!');
			return;
		}

		const onVisibilityChange = () => setIsHidden(quickAccessWindow.document.hidden);
		quickAccessWindow.addEventListener('visibilitychange', onVisibilityChange);
		return () => {
			quickAccessWindow.removeEventListener('visibilitychange', onVisibilityChange);
		};
	}, []);

	return !isHidden;
}
