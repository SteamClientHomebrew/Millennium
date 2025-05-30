import { Dropdown, Field, IconsModule, pluginSelf, Toggle } from '@steambrew/client';
import React from 'react';
import { locale } from '../../../locales';
import { OnMillenniumUpdate } from '../../types';
import { RenderAccentColorPicker } from '../../components/AccentColorPicker';
import { useMillenniumState, useUpdateConfig } from '../../config-provider';
import { DesktopTooltip, SettingsDialogSubHeader } from '../../components/SteamComponents';
import { AppConfig } from '../../AppConfig';

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
		{ label: locale.eOnMillenniumUpdateAutoInstall, data: OnMillenniumUpdate.AUTO_INSTALL },
	];

	return (
		<>
			<SettingsDialogSubHeader style={{ marginTop: '20px' }}>{locale.headerOnStartup}</SettingsDialogSubHeader>

			<Field label={locale.optionCheckForMillenniumUpdates}>
				<Toggle value={config.general.checkForMillenniumUpdates} onChange={(e) => handleChange('checkForMillenniumUpdates', e)} />
			</Field>

			<Field label={locale.optionCheckForThemeAndPluginUpdates} bottomSeparator="none">
				<Toggle value={config.general.checkForPluginAndThemeUpdates} onChange={(e) => handleChange('checkForPluginAndThemeUpdates', e)} />
			</Field>

			<SettingsDialogSubHeader style={{ marginTop: '20px' }}>{locale.headerUpdates}</SettingsDialogSubHeader>

			<Field
				label={locale.optionWhenAnUpdateForMillenniumIsAvailable}
				bottomSeparator="none"
				disabled={!config.general.checkForMillenniumUpdates}
				icon={
					!config.general.checkForMillenniumUpdates && (
						<DesktopTooltip toolTipContent={locale.tooltipCheckForMillenniumUpdates} direction="top" style={{ height: '16px', width: '20px', marginTop: '-5px' }}>
							<IconsModule.ExclamationPoint color={'#ffc82c'} />
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

			<SettingsDialogSubHeader style={{ marginTop: '20px' }}>{locale.headerNotifications}</SettingsDialogSubHeader>

			<Field label={locale.optionWhenAPluginOrThemeUpdateIsAvailable} bottomSeparator="none">
				<Toggle value={config.general.shouldShowThemePluginUpdateNotifications} onChange={(e) => handleChange('shouldShowThemePluginUpdateNotifications', e)} />
			</Field>

			<SettingsDialogSubHeader style={{ marginTop: '20px' }}>{locale.headerThemes}</SettingsDialogSubHeader>

			<Field label={locale.themePanelInjectJavascript}>
				<Toggle value={config.general.injectJavascript} onChange={(e) => handleChange('injectJavascript', e)} />
			</Field>
			<Field label={locale.themePanelInjectCSS}>
				<Toggle value={config.general.injectCSS} onChange={(e) => handleChange('injectCSS', e)} />
			</Field>

			<RenderAccentColorPicker />

			<SettingsDialogSubHeader style={{ marginTop: '20px' }}>{locale.strAbout}</SettingsDialogSubHeader>

			<Field label={locale.strAboutVersion}>{pluginSelf.version}</Field>
			<Field label={'Client API version'}>{window.MILLENNIUM_FRONTEND_LIB_VERSION}</Field>
			<Field label={'Browser API version'}>{window.MILLENNIUM_BROWSER_LIB_VERSION}</Field>
			<Field label={locale.strAboutBuildDate}>{new Date(pluginSelf.buildDate).toLocaleString(navigator.language)}</Field>
			<Field label={'Loader build date'} bottomSeparator="none">
				{new Date(window.MILLENNIUM_LOADER_BUILD_DATE).toLocaleString(navigator.language)}
			</Field>
		</>
	);
};
