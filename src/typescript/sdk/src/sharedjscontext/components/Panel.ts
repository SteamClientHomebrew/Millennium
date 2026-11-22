import { FC, ReactNode } from 'react';

import { Export, findModuleDetailsByExport, sourceOf } from '../module-registry';
import { nativeInternal } from '../internals';

// TODO where did this go?
// export const Panel: FC<{ children?: ReactNode; }> = findModuleExport((e: Export) => {
//   if (typeof mod !== 'object' || !mod.__esModule) return undefined;
//   return mod.Panel;
// });

export interface PanelSectionProps {
	title?: string;
	spinner?: boolean;
	children?: ReactNode;
}

export interface PanelSectionRowProps {
	children?: ReactNode;
}

const nativePanelSection = nativeInternal('PanelSection');
const [mod, legacyPanelSection] = nativePanelSection ? [undefined, undefined] : findModuleDetailsByExport((e: Export) => sourceOf(e).includes('.PanelSection'));

/** @component React Components */
export const PanelSection = (nativePanelSection ?? legacyPanelSection) as FC<PanelSectionProps>;

/** @component React Components */
export const PanelSectionRow = (nativeInternal('PanelSectionRow') ??
	(mod ? Object.values(mod).filter((exp: any) => !sourceOf(exp).includes('.PanelSection'))[0] : undefined)) as FC<PanelSectionRowProps>;
