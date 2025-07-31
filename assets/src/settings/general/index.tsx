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

import { DialogControlsSection, Dropdown, Field, IconsModule, pluginSelf, Toggle } from '@steambrew/client';
import React from 'react';
import { locale } from '../../../locales';
import { OnMillenniumUpdate, OSType } from '../../types';
import { RenderAccentColorPicker } from '../../components/AccentColorPicker';
import { useMillenniumState, useUpdateConfig } from '../../config-provider';
import { DesktopTooltip, SettingsDialogSubHeader } from '../../components/SteamComponents';
import { AppConfig } from '../../AppConfig';
import { deferredSettingLabelClasses } from '../../utils/classes';

export const GeneralViewModal: React.FC = () => {
	const config = useMillenniumState();
	const updateConfig = useUpdateConfig();

	const handleChange = <K extends keyof AppConfig['general']>(key: K, value: AppConfig['general'][K]) => {
		updateConfig((draft) => {
			draft.general[key] = value;
		});
	};

	const OnMillenniumUpdateOpts = [
		{ label: locale.eOnMillenniumUpdateDoNothing, data: OnMillenniumUpdate.DO_NOTHING },
		{ label: locale.eOnMillenniumUpdateNotify, data: OnMillenniumUpdate.NOTIFY },
	];

	if (pluginSelf?.platformType === OSType.Windows) {
		OnMillenniumUpdateOpts.push({ label: locale.eOnMillenniumUpdateAutoInstall, data: OnMillenniumUpdate.AUTO_INSTALL });
	}

	return (
		<>
			<DialogControlsSection>
				<SettingsDialogSubHeader>{locale.headerOnStartup}</SettingsDialogSubHeader>

				<Field label={locale.optionCheckForMillenniumUpdates}>
					<Toggle value={config.general.checkForMillenniumUpdates} onChange={(e) => handleChange('checkForMillenniumUpdates', e)} />
				</Field>

				<Field label={locale.optionCheckForThemeAndPluginUpdates} bottomSeparator="none">
					<Toggle value={config.general.checkForPluginAndThemeUpdates} onChange={(e) => handleChange('checkForPluginAndThemeUpdates', e)} />
				</Field>
			</DialogControlsSection>

			<DialogControlsSection>
				<SettingsDialogSubHeader>{locale.headerUpdates}</SettingsDialogSubHeader>

				<Field
					label={locale.optionWhenAnUpdateForMillenniumIsAvailable}
					bottomSeparator="none"
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
						strDefaultLabel={OnMillenniumUpdateOpts.find((opt) => opt.data === config.general.onMillenniumUpdate)?.label}
					/>
				</Field>
			</DialogControlsSection>

			<DialogControlsSection>
				<SettingsDialogSubHeader>{locale.headerNotifications}</SettingsDialogSubHeader>

				<Field label={locale.optionWhenAPluginOrThemeUpdateIsAvailable} bottomSeparator="none">
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
				<SettingsDialogSubHeader>{locale.strAbout}</SettingsDialogSubHeader>

				<Field label={locale.strAboutVersion}>{pluginSelf.version}</Field>
				<Field label={'Client API version'}>{window.MILLENNIUM_FRONTEND_LIB_VERSION}</Field>
				<Field label={'Browser API version'}>{window.MILLENNIUM_BROWSER_LIB_VERSION}</Field>
				<Field label={locale.strAboutBuildDate}>{new Date(pluginSelf.buildDate).toLocaleString(navigator.language)}</Field>
				<Field label={'Loader build date'} bottomSeparator="none">
					{new Date(window.MILLENNIUM_LOADER_BUILD_DATE).toLocaleString(navigator.language)}
				</Field>
			</DialogControlsSection>
		</>
	);
};
