import { FC, ReactNode } from 'react';

import { Export, findModuleExport } from '../webpack';

export interface ControlsListProps {
	alignItems?: 'left' | 'right' | 'center';
	spacing?: 'standard' | 'extra';
	children?: ReactNode;
}
/** @component React Components */
export const ControlsList: FC<ControlsListProps> = findModuleExport(
	(e: Export) => e?.toString && e.toString().includes('().ControlsListChild') && e.toString().includes('().ControlsListOuterPanel'),
);
