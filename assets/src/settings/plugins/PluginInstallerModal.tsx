import { ConfirmModal, pluginSelf, showModal, ShowModalResult, SuspensefulImage, TextField } from '@steambrew/client';
import React, { Component, useEffect } from 'react';
import { Utils } from '../../utils';

import FindPluginIdGif from '../../../static/plugin_id.gif';
import { PLUGINS_URL } from '../../utils/globals';
import { Installer } from '../general/Installer';

interface PluginIdModalProps {
	tutorialImageUrl: string;
	installer: Installer;
	modal: ShowModalResult;
}

function PluginIdModal({ tutorialImageUrl, installer, modal }: PluginIdModalProps) {
	const [installID, setInstallID] = React.useState(String());

	return (
		<ConfirmModal
			strTitle="Enter an ID"
			strDescription={
				<>
					Install a user plugin from an ID. These ID's can be found after selecting a plugin at <Utils.URLComponent url={PLUGINS_URL} />
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
		img.src = url;
		img.onload = () => resolve(img);
		img.onerror = (err) => reject(err);
	});
}

export async function showInstallPluginModal() {
	let modal: ShowModalResult;
	const tutorialImageUrl = [await Utils.GetPluginAssetUrl(), FindPluginIdGif].join('/');
	const installer = new Installer();

	await cacheImage(tutorialImageUrl);

	const WrappedModal = () => {
		const [modalInstance, setModalInstance] = React.useState<ShowModalResult | null>(null);

		useEffect(() => {
			setModalInstance(modal);
		}, []);

		return <PluginIdModal tutorialImageUrl={tutorialImageUrl} installer={installer} modal={modalInstance} />;
	};

	modal = showModal(<WrappedModal />, pluginSelf.mainWindow, {
		bNeverPopOut: true,
		popupHeight: 650,
		popupWidth: 750,
	});
}
