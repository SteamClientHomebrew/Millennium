import { ComponentType, FC, ReactNode, createContext, useContext, useEffect, useState } from 'react';
import type { RouteProps } from 'react-router';
import { EUIMode } from '../../globals/steam-client/shared';

export interface RouterEntry {
	props: Omit<RouteProps, 'path' | 'children'>;
	component: ComponentType;
}

export type RoutePatch = (route: RouteProps) => RouteProps;

interface PublicMillenniumRouterState {
	routes: Map<string, RouterEntry>;
	routePatches: Map<EUIMode, Map<string, Set<RoutePatch>>>;
}

export class MillenniumRouterState {
	private _routes = new Map<string, RouterEntry>();
	// Update when support for new UIModes is added
	private _routePatches = new Map<EUIMode, Map<string, Set<RoutePatch>>>([
		[EUIMode.GamePad, new Map()],
		[EUIMode.Desktop, new Map()],
	]);

	public eventBus = new EventTarget();

	publicState(): PublicMillenniumRouterState {
		return { routes: this._routes, routePatches: this._routePatches };
	}

	addRoute(path: string, component: RouterEntry['component'], props: RouterEntry['props'] = {}) {
		this._routes.set(path, { props, component });
		this.notifyUpdate();
	}

	addPatch(path: string, patch: RoutePatch, uiMode: EUIMode) {
		const patchesForMode = this._routePatches.get(uiMode);
		if (!patchesForMode) throw new Error(`UI mode ${uiMode} not supported.`);
		let patchList = patchesForMode.get(path);
		if (!patchList) {
			patchList = new Set();
			patchesForMode.set(path, patchList);
		}
		patchList.add(patch);
		this.notifyUpdate();
		return patch;
	}

	removePatch(path: string, patch: RoutePatch, uiMode: EUIMode) {
		const patchesForMode = this._routePatches.get(uiMode);
		if (!patchesForMode) throw new Error(`UI mode ${uiMode} not supported.`);
		const patchList = patchesForMode.get(path);
		patchList?.delete(patch);
		if (patchList?.size == 0) {
			patchesForMode.delete(path);
		}
		this.notifyUpdate();
	}

	removeRoute(path: string) {
		this._routes.delete(path);
		this.notifyUpdate();
	}

	private notifyUpdate() {
		this.eventBus.dispatchEvent(new Event('update'));
	}
}

interface MillenniumRouterStateContext extends PublicMillenniumRouterState {
	addRoute(path: string, component: RouterEntry['component'], props: RouterEntry['props']): void;
	addPatch(path: string, patch: RoutePatch, uiMode?: EUIMode): RoutePatch;
	removePatch(path: string, patch: RoutePatch, uiMode?: EUIMode): void;
	removeRoute(path: string): void;
}

const MillenniumRouterStateContext = createContext<MillenniumRouterStateContext>(null as any);

export const useMillenniumRouterState = () => useContext(MillenniumRouterStateContext);

interface Props {
	millenniumRouterState: MillenniumRouterState;
	children: ReactNode;
}

export const MillenniumRouterStateContextProvider: FC<Props> = ({ children, millenniumRouterState: millenniumRouterState }) => {
	const [publicMillenniumRouterState, setPublicMillenniumRouterState] = useState<PublicMillenniumRouterState>({
		...millenniumRouterState.publicState(),
	});

	useEffect(() => {
		function onUpdate() {
			setPublicMillenniumRouterState({ ...millenniumRouterState.publicState() });
		}

		millenniumRouterState.eventBus.addEventListener('update', onUpdate);

		return () => millenniumRouterState.eventBus.removeEventListener('update', onUpdate);
	}, []);

	const addRoute = millenniumRouterState.addRoute.bind(millenniumRouterState);
	const addPatch = millenniumRouterState.addPatch.bind(millenniumRouterState);
	const removePatch = millenniumRouterState.removePatch.bind(millenniumRouterState);
	const removeRoute = millenniumRouterState.removeRoute.bind(millenniumRouterState);

	return (
		<MillenniumRouterStateContext.Provider value={{ ...publicMillenniumRouterState, addRoute, addPatch, removePatch, removeRoute }}>
			{children}
		</MillenniumRouterStateContext.Provider>
	);
};
