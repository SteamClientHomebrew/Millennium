import { ReactNode, FC } from 'react';

import { Export, findModuleExport, sourceOf } from '../module-registry';
import { ItemProps } from './Item';
import { createPropListRegex } from '../utils';
import { nativeInternal } from '../internals';

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
export const ProgressBar = (nativeInternal('ProgressBar') ??
  findModuleExport((e: Export) => /\.ProgressBar,[\w$]+=="standard/.test(sourceOf(e)))) as FC<ProgressBarProps>;
/** @component React Components */
export const ProgressBarWithInfo = (findModuleExport((e: Export) => sourceOf(e).includes('.ProgressBarFieldStatus') && sourceOf(e).includes('.OperationText')) ??
  nativeInternal('ProgressBarItem')) as FC<ProgressBarWithInfoProps>;

const progressBarItemRegex = createPropListRegex(['indeterminate', 'nTransitionSec', 'nProgress']);
/** @component React Components */
export const ProgressBarItem = (nativeInternal('ProgressBarItem') ?? findModuleExport((e: Export) => progressBarItemRegex.test(sourceOf(e)))) as FC<ProgressBarItemProps>;
