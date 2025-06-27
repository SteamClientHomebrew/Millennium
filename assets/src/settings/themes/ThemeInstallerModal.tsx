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
}

function ThemeIdModal({ tutorialImageUrl, installer, modal }: ThemeIdModalProps) {
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
				installer.OpenInstallPrompt(installID);
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

export async function showInstallThemeModal() {
	let modal: ShowModalResult;
	const tutorialImageUrl = [await Utils.GetPluginAssetUrl(), FindThemeIdGif].join('/');
	const installer = new Installer();

	await cacheImage(tutorialImageUrl);

	const WrappedModal = () => {
		const [modalInstance, setModalInstance] = React.useState<ShowModalResult | null>(null);

		useEffect(() => {
			setModalInstance(modal);
		}, []);

		return <ThemeIdModal tutorialImageUrl={tutorialImageUrl} installer={installer} modal={modalInstance} />;
	};

	modal = showModal(<WrappedModal />, pluginSelf.mainWindow, {
		bNeverPopOut: true,
		popupHeight: 650,
		popupWidth: 750,
	});
}
