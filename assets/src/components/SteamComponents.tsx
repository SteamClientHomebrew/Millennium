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

export const BBCodeParser: FC<BBCodeParserProps> = findModuleExport((m) => typeof m === 'function' && m.toString().includes('this.ElementAccumulator'));

interface SettingsDialogSubHeaderProps extends React.HTMLProps<HTMLDivElement> {
	children: ReactNode;
}

export const SettingsDialogSubHeader: React.FC<SettingsDialogSubHeaderProps> = ({ children }) => <div className="SettingsDialogSubHeader">{children}</div>;

export const Separator: React.FC = () => <div className={fieldClasses.StandaloneFieldSeparator} />;

export const DesktopTooltip = findModuleDetailsByExport(
	(m) => m.toString().includes(`divProps`) && m.toString().includes(`tooltipProps`) && m.toString().includes(`toolTipContent`) && m.toString().includes(`tool-tip-source`),
)[1];

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
