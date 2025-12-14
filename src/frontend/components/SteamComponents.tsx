/**
 * ==================================================
 *   _____ _ _ _             _
 *  |     |_| | |___ ___ ___|_|_ _ _____
 *  | | | | | | | -_|   |   | | | |     |
 *  |_|_|_|_|_|_|___|_|_|_|_|_|___|_|_|_|
 *
 * ==================================================
 *
 * Copyright (c) 2025 Project Millennium
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

import React, { ReactNode } from 'react';
import { findModuleDetailsByExport, findModuleExport, ModalPosition } from '@steambrew/client';
import { FC } from 'react';
import { fieldClasses } from '../utils/classes';

interface BBCodeParserProps {
	bShowShortSpeakerInfo?: boolean;
	event?: any;
	languageOverride?: any;
	showErrorInfo?: boolean;
	text?: string;
}

export const BBCodeParser: FC<BBCodeParserProps> = findModuleExport(
	(m) => typeof m === 'function' && m?.toString?.().includes('ElementAccumulator') && m?.toString?.().includes('parser.ParseBBCode'),
) as FC<BBCodeParserProps>;

interface SettingsDialogSubHeaderProps extends React.HTMLProps<HTMLDivElement> {
	children: ReactNode;
}

export const SettingsDialogSubHeader: React.FC<SettingsDialogSubHeaderProps> = ({ children }) => <div className="SettingsDialogSubHeader">{children}</div>;

export const Separator: React.FC = () => <div className={fieldClasses.StandaloneFieldSeparator} />;

export const DesktopTooltip = findModuleDetailsByExport(
	(m) =>
		m?.toString?.()?.includes(`divProps`) &&
		m?.toString?.()?.includes(`tooltipProps`) &&
		m?.toString?.()?.includes(`toolTipContent`) &&
		m?.toString?.()?.includes(`tool-tip-source`),
)?.[1];

export const createWindowContext = findModuleDetailsByExport(
	(m) =>
		m?.toString?.()?.includes('current?.params.bNoInitialShow') &&
		m?.toString?.()?.includes('current?.params.bNoFocusOnShow') &&
		m?.toString?.()?.includes('current.m_callbacks'),
)?.[1];

export const ModalManagerInstance = findModuleDetailsByExport((m) => {
	return m?.hasOwnProperty?.('m_mapModalManager');
})?.[1];

export const windowHandler = findModuleDetailsByExport(
	(m) =>
		m?.toString?.()?.includes('SetCurrentLoggedInAccountID') && m?.toString?.()?.includes('bIgnoreSavedDimensions') && m?.toString?.()?.includes('updateParamsBeforeShow'),
)?.[1];

export const RenderWindowTitle = findModuleDetailsByExport((m) => m?.toString?.()?.includes('title-bar-actions window-controls'))?.[1];

export const g_ModalManager = findModuleDetailsByExport((m) => {
	return m?.toString?.()?.includes('useContext') && m?.toString?.()?.includes('ModalManager') && m?.length === 0;
})?.[1];

export const OwnerWindowRef = findModuleDetailsByExport(
	(m) =>
		m?.toString?.()?.includes('ownerWindow') &&
		m?.toString?.()?.includes('children') &&
		m?.toString?.()?.includes('useMemo') &&
		m?.toString?.()?.includes('Provider') &&
		!m?.toString?.()?.includes('refFocusNavContext'),
)?.[1];

export const popupModalManager: any = Object.values(findModuleDetailsByExport((m) => m?.toString()?.includes('GetContextMenuManagerFromWindow'))?.[0] || {})?.find(
	(obj) => obj && obj?.hasOwnProperty('m_mapManagers'),
);

export const contextMenuManager = findModuleDetailsByExport(
	(m) =>
		m?.toString?.()?.includes('CreateContextMenuInstance') &&
		m?.prototype?.hasOwnProperty('GetVisibleMenus') &&
		m?.prototype?.hasOwnProperty('GetAllMenus') &&
		m?.prototype?.hasOwnProperty('ShowMenu'),
)?.[1];

export const PopupModalHandler: any = Object.values(
	findModuleDetailsByExport((m) => m?.toString?.()?.includes('this.context.callbacks?.onDisabledItemSelected'))?.[0] || {},
)?.find((m) => m?.toString?.()?.includes('useId'));

interface GenericConfirmDialogProps {
	children: ReactNode;
}

export const GenericConfirmDialog = ({ children }: GenericConfirmDialogProps) => (
	<ModalPosition>
		<div className="DialogContent _DialogLayout GenericConfirmDialog _DialogCenterVertically">
			<div className="DialogContent_InnerWidth">{children}</div>
		</div>
	</ModalPosition>
);
