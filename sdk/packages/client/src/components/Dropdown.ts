import { ReactNode, FC } from 'react';

import { CommonUIModule } from '../webpack';
import { ItemProps } from './Item';
import { createPropListRegex } from '../utils';

export interface SingleDropdownOption {
	data: any;
	label: ReactNode;

	options?: never;
}

export interface MultiDropdownOption {
	label: ReactNode;
	options: DropdownOption[];

	data?: never;
}

export type DropdownOption = SingleDropdownOption | MultiDropdownOption;

export interface DropdownMenuPositionOptions {
	[_: string]: unknown;
	bMatchWidth?: boolean;
}

export interface DropdownProps {
	rgOptions: DropdownOption[];
	selectedOption: any;
	disabled?: boolean;
	onMenuWillOpen?(showMenu: () => void): void;
	onMenuOpened?(): void;
	onChange?(data: SingleDropdownOption): void;
	contextMenuPositionOptions?: DropdownMenuPositionOptions;
	menuLabel?: string;
	strDefaultLabel?: string;
	renderButtonValue?(element: ReactNode): ReactNode;
	focusable?: boolean;
}

/** @component React Components */
export const Dropdown = Object.values(CommonUIModule).find((mod: any) => mod?.prototype?.SetSelectedOption && mod?.prototype?.BuildMenu) as FC<DropdownProps>;

export interface DropdownItemProps extends DropdownProps, ItemProps {}

const dropdownItemRegex = createPropListRegex(['dropDownControlRef', 'description'], false);
export const DropdownItem = Object.values(CommonUIModule).find((mod: any) => mod?.toString && dropdownItemRegex.test(mod.toString())) as FC<DropdownItemProps>;
