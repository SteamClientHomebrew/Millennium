import { ReactNode, FC } from 'react';

import { Export, findModuleExport } from '../webpack';
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
	title?: string;
	pages: (SidebarNavigationPage | 'separator')[];
	showTitle?: boolean;
	disableRouteReporting?: boolean;
	page?: string;
	onPageRequested?: (page: string) => void;
}

const sidebarNavigationRegex = createPropListRegex(['pages', 'fnSetNavigateToPage', 'disableRouteReporting']);
/** @component React Components */
export const SidebarNavigation = findModuleExport((e: Export) => e?.toString && sidebarNavigationRegex.test(e.toString())) as FC<SidebarNavigationProps>;
