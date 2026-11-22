import { FC, ReactNode } from 'react';

import { Export, findModuleExport, sourceOf } from '../module-registry';

export interface ControlsListProps {
	alignItems?: 'left' | 'right' | 'center';
	spacing?: 'standard' | 'extra';
	children?: ReactNode;
}
/** @component React Components */
export const ControlsList: FC<ControlsListProps> = findModuleExport(
	(e: Export) => sourceOf(e).includes('().ControlsListChild') && sourceOf(e).includes('().ControlsListOuterPanel'),
);
