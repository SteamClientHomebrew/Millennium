import { ReactNode, FC } from 'react';

import { Export, findModuleExport, sourceOf } from '../module-registry';
import { nativeInternal } from '../internals';
import { createPropListRegex } from '../utils';

export interface SidebarNavigationPage {
	title: ReactNode;
	content: ReactNode;
	icon?: ReactNode;
	visible?: boolean;
	hideTitle?: boolean;
	identifier?: string;
	route?: string;
	link?: string;
	padding?: 'none' | 'compact';
}

export interface SidebarNavigationProps {
	className?: string;
	title?: string;
	pages: (SidebarNavigationPage | 'separator')[];
	showTitle?: boolean;
	disableRouteReporting?: boolean;
	page?: string;
	onPageRequested?: (page: string) => void;
}

const sidebarNavigationRegex = createPropListRegex(['pages', 'fnSetNavigateToPage', 'disableRouteReporting']);
const sidebarNavigationStylesheetRegex = createPropListRegex(['pages', 'stylesheet']);
/** @component React Components */
export const SidebarNavigation = (nativeInternal('SidebarNavigation') ??
	findModuleExport((e: Export) => sidebarNavigationRegex.test(sourceOf(e)) || sidebarNavigationStylesheetRegex.test(sourceOf(e)))) as FC<SidebarNavigationProps>;
