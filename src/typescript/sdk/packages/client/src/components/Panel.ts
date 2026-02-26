import { FC, ReactNode } from 'react';

import { Export, findModuleDetailsByExport } from '../webpack';

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

const [mod, panelSection] = findModuleDetailsByExport((e: Export) => e.toString()?.includes('.PanelSection'));

/** @component React Components */
export const PanelSection = panelSection as FC<PanelSectionProps>;

export interface PanelSectionRowProps {
	children?: ReactNode;
}
/** @component React Components */
export const PanelSectionRow = Object.values(mod).filter((exp: any) => !exp?.toString()?.includes('.PanelSection'))[0] as FC<PanelSectionRowProps>;
