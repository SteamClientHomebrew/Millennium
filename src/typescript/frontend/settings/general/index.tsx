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

import { ConfirmModal, DialogButton, DialogControlsSection, Dropdown, Field, IconsModule, pluginSelf, showModal, ShowModalResult, TextField, Toggle } from '@steambrew/sdk';
import React, { useEffect } from 'react';
import { locale } from '../../utils/localization-manager';
import { MillenniumUpdateChannel, OnMillenniumUpdate, OSType } from '../../types';
import { RenderAccentColorPicker } from '../../components/AccentColorPicker';
import { useMillenniumState, useUpdateConfig } from '../../utils/config-provider';
import { DesktopTooltip, SettingsDialogSubHeader } from '../../components/SteamComponents';
import { AppConfig } from '../../utils/AppConfig';
import { deferredSettingLabelClasses, settingsClasses } from '../../utils/classes';
import { refetchMillenniumUpdates } from '../updates/useUpdateContext';

export const GeneralViewModal: React.FC = () => {
	const configOrNull = useMillenniumState();
	const updateConfig = useUpdateConfig();

	if (!configOrNull) return null;
	const config = configOrNull;

	const handleChange = <K extends keyof AppConfig['general']>(key: K, value: AppConfig['general'][K]) => {
		updateConfig((draft) => {
			draft.general[key] = value;
		});
	};

	const handleNetworkChange = <K extends keyof AppConfig['network']>(key: K, value: AppConfig['network'][K]) => {
		updateConfig((draft) => {
			draft.network[key] = value;
		});
	};

	const showCredentialModal = (title: string, placeholder: string, isPassword: boolean, onConfirm: (value: string) => void) => {
		let modal: ShowModalResult;

		const Modal = () => {
			const [value, setValue] = React.useState('');
			const [modalInstance, setModalInstance] = React.useState<ShowModalResult | null>(null);
			useEffect(() => { setModalInstance(modal); }, []);

			return (
				<ConfirmModal
					strTitle={title}
					strDescription={
						<TextField
							// @ts-ignore
							placeholder={placeholder}
							type={isPassword ? 'password' : 'text'}
							value={value}
							onChange={(e) => setValue(e.target.value)}
						/>
					}
					bHideCloseIcon={true}
					strOKButtonText={locale.optionSet}
					onOK={() => { onConfirm(value); modalInstance?.Close(); }}
					onCancel={() => modalInstance?.Close()}
				/>
			);
		};

		modal = showModal(<Modal />, pluginSelf.mainWindow, { bNeverPopOut: true, popupHeight: 250, popupWidth: 500 });
	};

	const OnMillenniumUpdateOpts = [
		{ label: locale.eOnMillenniumUpdateDoNothing, data: OnMillenniumUpdate.DO_NOTHING },
		{ label: locale.eOnMillenniumUpdateNotify, data: OnMillenniumUpdate.NOTIFY },
	];

	if (pluginSelf?.platformType === OSType.Windows) {
		OnMillenniumUpdateOpts.push({ label: locale.eOnMillenniumUpdateAutoInstall, data: OnMillenniumUpdate.AUTO_INSTALL });
	}

	const millenniumUpdateChannel = [
		{ label: locale.updatePanelStableChannel, data: MillenniumUpdateChannel.STABLE },
		{ label: locale.updatePanelBetaChannel, data: MillenniumUpdateChannel.BETA },
	];

	return (
		<>
			<DialogControlsSection>
				<SettingsDialogSubHeader>{locale.headerOnStartup}</SettingsDialogSubHeader>

				<Field label={locale.optionCheckForMillenniumUpdates}>
					<Toggle value={config.general.checkForMillenniumUpdates} onChange={(e) => handleChange('checkForMillenniumUpdates', e)} />
				</Field>

				<Field label={locale.optionCheckForThemeAndPluginUpdates}>
					<Toggle value={config.general.checkForPluginAndThemeUpdates} onChange={(e) => handleChange('checkForPluginAndThemeUpdates', e)} />
				</Field>
			</DialogControlsSection>

			<DialogControlsSection>
				<SettingsDialogSubHeader>{locale.headerUpdates}</SettingsDialogSubHeader>

				<Field
					label={locale.optionWhenAnUpdateForMillenniumIsAvailable}
					disabled={!config.general.checkForMillenniumUpdates}
					icon={
						!config.general.checkForMillenniumUpdates && (
							<DesktopTooltip toolTipContent={locale.tooltipCheckForMillenniumUpdates} direction="top">
								<IconsModule.ExclamationPoint className={deferredSettingLabelClasses.Icon} />
							</DesktopTooltip>
						)
					}
				>
					<Dropdown
						disabled={!config.general.checkForMillenniumUpdates}
						rgOptions={OnMillenniumUpdateOpts}
						selectedOption={OnMillenniumUpdateOpts.findIndex((opt) => opt.data === config.general.onMillenniumUpdate)}
						onChange={(e) => handleChange('onMillenniumUpdate', e.data)}
						contextMenuPositionOptions={{ bMatchWidth: false }}
						strDefaultLabel={OnMillenniumUpdateOpts.find((opt) => opt.data === config.general.onMillenniumUpdate)?.label ?? ''}
					/>
				</Field>

				<Field
					label={locale.updatePanelUpdateChannel}
					description={locale.updatePanelUpdateChannelTooltip}
					icon={
						config.general.millenniumUpdateChannel === MillenniumUpdateChannel.BETA && (
							<DesktopTooltip toolTipContent={locale.updatePanelBetaWarning} direction="top">
								<IconsModule.ExclamationPoint className={deferredSettingLabelClasses.Icon} />
							</DesktopTooltip>
						)
					}
				>
					<Dropdown
						rgOptions={millenniumUpdateChannel}
						selectedOption={millenniumUpdateChannel.findIndex((opt) => opt.data === config.general.millenniumUpdateChannel)}
						onChange={(e) => {
							handleChange('millenniumUpdateChannel', e.data);
							refetchMillenniumUpdates(e.data);
						}}
						contextMenuPositionOptions={{ bMatchWidth: false }}
						strDefaultLabel={millenniumUpdateChannel.find((opt) => opt.data === config.general.millenniumUpdateChannel)?.label ?? ''}
					/>
				</Field>
			</DialogControlsSection>

			<DialogControlsSection>
				<SettingsDialogSubHeader>{locale.headerNotifications}</SettingsDialogSubHeader>

				<Field label={locale.optionWhenAPluginOrThemeUpdateIsAvailable}>
					<Toggle value={config.general.shouldShowThemePluginUpdateNotifications} onChange={(e) => handleChange('shouldShowThemePluginUpdateNotifications', e)} />
				</Field>
			</DialogControlsSection>

			<DialogControlsSection>
				<SettingsDialogSubHeader>{locale.headerThemes}</SettingsDialogSubHeader>

				<Field label={locale.themePanelInjectJavascript}>
					<Toggle value={config.general.injectJavascript} onChange={(e) => handleChange('injectJavascript', e)} />
				</Field>
				<Field label={locale.themePanelInjectCSS}>
					<Toggle value={config.general.injectCSS} onChange={(e) => handleChange('injectCSS', e)} />
				</Field>

				<RenderAccentColorPicker />
			</DialogControlsSection>

			<DialogControlsSection>
				<SettingsDialogSubHeader>{locale.headerNetwork}</SettingsDialogSubHeader>

				<Field label={locale.optionProxyUrl} description={locale.optionProxyUrlDescription}>
					<TextField
						// @ts-ignore
						placeholder={locale.optionProxyUrlPlaceholder}
						value={config.network.proxy}
						onChange={(e) => handleNetworkChange('proxy', e.target.value)}
					/>
				</Field>

				<Field label={locale.optionProxyUsername} disabled={!config.network.proxy}>
					{config.network.proxyUsername && (
						<TextField disabled value={config.network.proxyUsername} />
					)}
					<DialogButton
						className={settingsClasses.SettingsDialogButton}
						disabled={!config.network.proxy}
						onClick={() => showCredentialModal(locale.optionProxyUsername, 'Username', false, (v) => handleNetworkChange('proxyUsername', v))}
					>
						{config.network.proxyUsername ? locale.optionChangeUsername : locale.optionSetUsername}
					</DialogButton>
					{config.network.proxyUsername && (
						<DialogButton
							className={settingsClasses.SettingsDialogButton}
							onClick={() => handleNetworkChange('proxyUsername', '')}
						>
							{locale.optionRemove}
						</DialogButton>
					)}
				</Field>

				<Field label={locale.optionProxyPassword} disabled={!config.network.proxy}>
					<DialogButton
						className={settingsClasses.SettingsDialogButton}
						disabled={!config.network.proxy}
						onClick={() => showCredentialModal(locale.optionProxyPassword, 'Password', true, (v) => handleNetworkChange('proxyPassword', v))}
					>
						{config.network.proxyPassword === '__SET__' ? locale.optionChangePassword : locale.optionSetPassword}
					</DialogButton>
					{config.network.proxyPassword === '__SET__' && (
						<DialogButton
							className={settingsClasses.SettingsDialogButton}
							onClick={() => handleNetworkChange('proxyPassword', '')}
						>
							{locale.optionRemove}
						</DialogButton>
					)}
				</Field>
			</DialogControlsSection>

			<DialogControlsSection>
				<SettingsDialogSubHeader>{locale.strAbout}</SettingsDialogSubHeader>

				<Field label={locale.strAboutVersion}>{pluginSelf.version}</Field>
				<Field label={locale.strClientApiVersion}>{window.MILLENNIUM_FRONTEND_LIB_VERSION}</Field>
				<Field label={locale.strBrowserApiVersion}>{window.MILLENNIUM_BROWSER_LIB_VERSION}</Field>
				<Field label={locale.strAboutBuildDate}>{new Date(pluginSelf.buildDate ?? '').toLocaleString(navigator.language)}</Field>
				<Field label={locale.strLoaderBuildDate}>{new Date(window.MILLENNIUM_LOADER_BUILD_DATE ?? '').toLocaleString(navigator.language)}</Field>
			</DialogControlsSection>
		</>
	);
};
