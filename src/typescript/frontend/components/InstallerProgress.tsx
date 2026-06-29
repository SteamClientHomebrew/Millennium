/**
 * ==================================================
 *   _____ _ _ _             _
 *  |     |_| | |___ ___ ___|_|_ _ _____
 *  | | | | | | | -_|   |   | | | |     |
 *  |_|_|_|_|_|_|___|_|_|_|_|_|___|_|_|_|
 *
 * ==================================================
 *
 * Copyright (c) 2026 Project Millennium
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

import { ConfirmModal, ProgressBar } from '@steambrew/sdk';
import React from 'react';
import { locale, tryResolveLocale } from '../utils/localization-manager';
import Styles from '../utils/styles';

export interface RendererProps {
	onInstallComplete: () => React.ReactNode;
	onProgressUpdate: ({ status }: { status: string }) => React.ReactElement;
	opId: number;
}

export const OnProgressUpdate = ({ status }: { status: string }) => {
	const RenderBody = () => (
		<>
			<Styles />
			<style>
				{`.MillenniumInstallerDialog .DialogButton { display: none; }
                .MillenniumInstallerDialog_ProgressStatus { font-weight: 500; float: right; margin-bottom: 10px; }
                .DialogBodyText { margin-bottom: unset; }`}
			</style>
			<div className="MillenniumInstallerDialog_ProgressStatus">{tryResolveLocale(status)}</div>
			<ProgressBar
				/* @ts-ignore */
				className="MillenniumInstallerDialog_ProgressBar"
				indeterminate
				nProgress={0}
			/>
		</>
	);

	return (
		<ConfirmModal
			className="MillenniumInstallerDialog"
			strTitle={locale.strInstallProgress}
			strDescription={<RenderBody />}
			onCancel={() => {}}
			bHideCloseIcon
			bAlertDialog
			bOKDisabled
			bCancelDisabled
			bDisableBackgroundDismiss
		/>
	);
};
