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

import { ConfirmModal, pluginSelf, showModal, ShowModalResult, SuspensefulImage, TextField } from '@steambrew/client';
import React, { Component, useEffect } from 'react';
import { Utils } from '../../utils';

import FindThemeIdGif from '../../../static/theme_id.gif';
import { THEMES_URL } from '../../utils/globals';
import { Installer } from '../general/Installer';

interface ThemeIdModalProps {
	tutorialImageUrl: string;
	installer: Installer;
	modal: ShowModalResult;
	refetchDataCb: () => void;
}

function ThemeIdModal({ tutorialImageUrl, installer, modal, refetchDataCb }: ThemeIdModalProps) {
	const [installID, setInstallID] = React.useState(String());

	return (
		<ConfirmModal
			strTitle="Enter an ID"
			strDescription={
				<>
					Install a user theme from an ID. These ID's can be found after selecting a theme at <Utils.URLComponent url={THEMES_URL} />
					<SuspensefulImage className="MillenniumInstallDialog_TutorialImage" src={tutorialImageUrl} />
					<TextField
						// @ts-ignore
						placeholder={'Enter an ID here...'}
						value={installID}
						onChange={(e) => setInstallID(e.target.value)}
					/>
				</>
			}
			bHideCloseIcon={true}
			onOK={() => {
				installer.OpenInstallPrompt(installID, refetchDataCb);
				modal?.Close();
			}}
			onCancel={() => {
				modal?.Close();
			}}
			strOKButtonText="Install"
		/>
	);
}

async function cacheImage(url: string) {
	return new Promise((resolve, reject) => {
		const img = new Image();
		img.referrerPolicy = 'origin';
		img.src = url;
		img.onload = () => resolve(img);
		img.onerror = (err) => reject(err);
	});
}

export async function showInstallThemeModal(refetchDataCb: () => void) {
	let modal: ShowModalResult;
	const tutorialImageUrl = [await Utils.GetPluginAssetUrl(), FindThemeIdGif].join('/');
	const installer = new Installer();

	await cacheImage(tutorialImageUrl);

	const WrappedModal = () => {
		const [modalInstance, setModalInstance] = React.useState<ShowModalResult | null>(null);

		useEffect(() => {
			setModalInstance(modal);
		}, []);

		return <ThemeIdModal refetchDataCb={refetchDataCb} tutorialImageUrl={tutorialImageUrl} installer={installer} modal={modalInstance} />;
	};

	modal = showModal(<WrappedModal />, pluginSelf.mainWindow, {
		bNeverPopOut: true,
		popupHeight: 650,
		popupWidth: 750,
	});
}
