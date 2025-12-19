import { FC, ReactNode } from 'react';

import { Export, findModuleByExport, findModuleDetailsByExport, findModuleExport } from '../webpack';
import { FooterLegendProps } from './FooterLegend';

interface PopupCreationOptions {
	/**
	 * Initially hidden, make it appear with {@link ContextMenuInstance.Show}.
	 */
	bCreateHidden?: boolean;

	bModal?: boolean;

	/**
	 * Document title.
	 */
	title?: string;
}

// Separate interface, since one of webpack module exports uses this exact object,
// so maybe it could be reused elsewhere.
interface MonitorOptions {
	targetMonitor: {
		flMonitorScale: number;
		nScreenLeft: number;
		nScreenTop: number;
		nScreenWidth: number;
		nScreenHeight: number;
	};
	flGamepadScale: number;
}

export interface ContextMenuPositionOptions extends PopupCreationOptions, Partial<MonitorOptions> {
	/**
	 * When {@link bForcePopup} is true, makes the window appear above everything else.
	 */
	bAlwaysOnTop?: boolean;

	/**
	 * Disables the mouse overlay, granting the ability to click anywhere while
	 * the menu's active.
	 */
	bDisableMouseOverlay?: boolean;

	/**
	 * Disables the {@link bPreferPopTop} behavior.
	 */
	bDisablePopTop?: boolean;

	bFitToWindow?: boolean;

	/**
	 * Forces the menu to open in a separate window instead of inside the parent.
	 */
	bForcePopup?: boolean;

	/**
	 * Like {@link bMatchWidth}, but don't shrink below the menu's minimum width.
	 */
	bGrowToElementWidth?: boolean;

	/**
	 * Match the parent's exact height.
	 */
	bMatchHeight?: boolean;

	/**
	 * Match the parent's exact width.
	 */
	bMatchWidth?: boolean;

	bNoFocusWhenShown?: boolean;

	/**
	 * Makes the menu **invisible**, instead of getting removed from the DOM.
	 */
	bRetainOnHide?: boolean;

	bScreenCoordinates?: boolean;

	/**
	 * Set to `true` to not account for the parent's width.
	 */
	bOverlapHorizontal?: boolean;

	/**
	 * Set to `true` to not account for the parent's height.
	 */
	bOverlapVertical?: boolean;

	/**
	 * Set to `true` to make the entire menu try to appear on the left side from
	 * the parent.
	 */
	bPreferPopLeft?: boolean;

	/**
	 * Set to `true` to make the entire menu try to appear above the parent.
	 */
	bPreferPopTop?: boolean;

	bShiftToFitWindow?: boolean;

	// different window creation flags (StandaloneContextMenu vs PopupContextMenu)
	bStandalone?: boolean;

	/**
	 * Class name **replacement** for the container element, i.e. it replaces the
	 * default class.
	 */
	strClassName?: string;
}

interface ContextMenuInstance {
	Hide(): void;
	Show(): void;
}

export const showContextMenu: (children: ReactNode, parent?: EventTarget, options?: ContextMenuPositionOptions) => ContextMenuInstance = findModuleExport(
	(e: Export) => typeof e === 'function' && e.toString().includes('GetContextMenuManagerFromWindow(') && e.toString().includes('.CreateContextMenuInstance('),
);

export interface MenuProps extends FooterLegendProps {
	label: string;
	onCancel?(): void;
	cancelText?: string;
	children?: ReactNode;
}

const MenuModule = findModuleDetailsByExport((e: Export) => e?.render?.toString()?.includes('bPlayAudio:') || (e?.prototype?.OnOKButton && e?.prototype?.OnMouseEnter));

export const Menu: FC<MenuProps> =
	findModuleExport((e: Export) => e?.prototype?.HideIfSubmenu && e?.prototype?.HideMenu) || // Legacy Menu
	(Object.values(MenuModule?.[0] ?? {}).find((e) => e?.toString()?.includes?.(`useId`) && e?.toString()?.includes?.(`labelId`)) as FC<MenuProps>); // New Menu 6/15/2025

export interface MenuGroupProps {
	label: string;
	disabled?: boolean;
	children?: ReactNode;
}

const MenuGoupModule = findModuleByExport(
	(e) => e?.prototype?.Focus && e?.prototype?.OnOKButton && e?.prototype?.render?.toString().includes?.(`"emphasis"==this.props.tone`),
);
export const MenuGroup: FC<MenuGroupProps> =
	MenuGoupModule && Object.values(MenuGoupModule).find((e: Export) => typeof e == 'function' && e?.toString?.()?.includes('bInGamepadUI:'));
export interface MenuItemProps extends FooterLegendProps {
	bInteractableItem?: boolean;
	onClick?(evt: Event): void;
	onSelected?(evt: Event): void;
	onMouseEnter?(evt: MouseEvent): void;
	onMoveRight?(): void;
	selected?: boolean;
	disabled?: boolean;
	bPlayAudio?: boolean;
	tone?: 'positive' | 'emphasis' | 'destructive';
	children?: ReactNode;
}

export const MenuItem: FC<MenuItemProps> = MenuModule?.[1];

export const MenuSeparator: FC = findModuleExport((e: Export) => typeof e === 'function' && /className:.+?\.ContextMenuSeparator/.test(e.toString()));
