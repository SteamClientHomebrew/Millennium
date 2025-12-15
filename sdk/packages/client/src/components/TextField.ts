import { ChangeEventHandler, HTMLAttributes, ReactNode, FC } from 'react';

import { CommonUIModule, Module } from '../webpack';

export interface TextFieldProps extends HTMLAttributes<HTMLInputElement> {
	label?: ReactNode;
	requiredLabel?: ReactNode;
	description?: ReactNode;
	disabled?: boolean;
	bShowCopyAction?: boolean;
	bShowClearAction?: boolean;
	bAlwaysShowClearAction?: boolean;
	bIsPassword?: boolean;
	rangeMin?: number;
	rangeMax?: number;
	mustBeNumeric?: boolean;
	mustBeURL?: boolean;
	mustBeEmail?: boolean;
	focusOnMount?: boolean;
	tooltip?: string;
	inlineControls?: ReactNode;
	onChange?: ChangeEventHandler<HTMLInputElement>;
	value?: string;
}

/** @component React Components */
export const TextField = Object.values(CommonUIModule).find((mod: Module) => mod?.validateUrl && mod?.validateEmail) as FC<TextFieldProps>;
