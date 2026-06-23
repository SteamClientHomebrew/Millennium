import { FC, ReactNode, createContext, useContext, useEffect, useState } from 'react';
import { EUIMode } from '../../globals/steam-client/shared';

interface PublicMillenniumGlobalComponentsState {
	components: Map<EUIMode, Map<string, FC>>;
}

export class MillenniumGlobalComponentsState {
	// TODO a set would be better
	private _components = new Map<EUIMode, Map<string, FC>>([
		[EUIMode.GamePad, new Map()],
		[EUIMode.Desktop, new Map()],
	]);

	public eventBus = new EventTarget();

	publicState(): PublicMillenniumGlobalComponentsState {
		return { components: this._components };
	}

	addComponent(path: string, component: FC, uiMode: EUIMode) {
		const components = this._components.get(uiMode);
		if (!components) throw new Error(`UI mode ${uiMode} not supported.`);

		components.set(path, component);
		this.notifyUpdate();
	}

	removeComponent(path: string, uiMode: EUIMode) {
		const components = this._components.get(uiMode);
		if (!components) throw new Error(`UI mode ${uiMode} not supported.`);

		components.delete(path);
		this.notifyUpdate();
	}

	private notifyUpdate() {
		this.eventBus.dispatchEvent(new Event('update'));
	}
}

interface MillenniumGlobalComponentsContext extends PublicMillenniumGlobalComponentsState {
	addComponent(path: string, component: FC, uiMode: EUIMode): void;
	removeComponent(path: string, uiMode: EUIMode): void;
}

const MillenniumGlobalComponentsContext = createContext<MillenniumGlobalComponentsContext>(null as any);

export const useMillenniumGlobalComponentsState = () => useContext(MillenniumGlobalComponentsContext);

interface Props {
	millenniumGlobalComponentsState: MillenniumGlobalComponentsState;
	children: ReactNode;
}

export const MillenniumGlobalComponentsStateContextProvider: FC<Props> = ({ children, millenniumGlobalComponentsState: millenniumGlobalComponentsState }) => {
	const [publicMillenniumGlobalComponentsState, setPublicMillenniumGlobalComponentsState] = useState<PublicMillenniumGlobalComponentsState>({
		...millenniumGlobalComponentsState.publicState(),
	});

	useEffect(() => {
		function onUpdate() {
			setPublicMillenniumGlobalComponentsState({ ...millenniumGlobalComponentsState.publicState() });
		}

		millenniumGlobalComponentsState.eventBus.addEventListener('update', onUpdate);

		return () => millenniumGlobalComponentsState.eventBus.removeEventListener('update', onUpdate);
	}, []);

	const addComponent = millenniumGlobalComponentsState.addComponent.bind(millenniumGlobalComponentsState);
	const removeComponent = millenniumGlobalComponentsState.removeComponent.bind(millenniumGlobalComponentsState);

	return (
		<MillenniumGlobalComponentsContext.Provider value={{ ...publicMillenniumGlobalComponentsState, addComponent, removeComponent }}>
			{children}
		</MillenniumGlobalComponentsContext.Provider>
	);
};
