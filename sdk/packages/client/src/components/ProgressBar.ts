import { ReactNode, FC } from 'react';

import { Export, findModuleExport } from '../webpack';
import { ItemProps } from './Item';
import { createPropListRegex } from '../utils';

export interface ProgressBarItemProps extends ItemProps {
	indeterminate?: boolean;
	nTransitionSec?: number;
	nProgress?: number;
	focusable?: boolean;
}

export interface ProgressBarProps {
	indeterminate?: boolean;
	nTransitionSec?: number;
	nProgress?: number;
	focusable?: boolean;
}

export interface ProgressBarWithInfoProps extends ProgressBarItemProps {
	sTimeRemaining?: ReactNode;
	sOperationText?: ReactNode;
}

/** @component React Components */
export const ProgressBar = findModuleExport((e: Export) => e?.toString()?.includes('.ProgressBar,"standard"==')) as FC<ProgressBarProps>;

/** @component React Components */
export const ProgressBarWithInfo = findModuleExport((e: Export) => e?.toString()?.includes('.ProgressBarFieldStatus},')) as FC<ProgressBarWithInfoProps>;

const progressBarItemRegex = createPropListRegex(['indeterminate', 'nTransitionSec', 'nProgress']);
/** @component React Components */
export const ProgressBarItem = findModuleExport((e: Export) => e?.toString && progressBarItemRegex.test(e.toString())) as FC<ProgressBarItemProps>;
