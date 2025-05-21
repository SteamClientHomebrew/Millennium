import { Dropdown, Field, Toggle } from '@steambrew/client';
import React from 'react';
import { locale } from '../../../locales';
import { OnMillenniumUpdate } from '../../types';
import { RenderAccentColorPicker } from '../../components/AccentColorPicker';
import { useMillenniumState, useUpdateConfig } from '../../config-provider';
import { SettingsDialogSubHeader } from '../../components/SteamComponents';
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
		{ label: 'Do nothing', data: OnMillenniumUpdate.DO_NOTHING },
		{ label: 'Notify me', data: OnMillenniumUpdate.NOTIFY },
		{ label: 'Automatically install', data: OnMillenniumUpdate.AUTO_INSTALL },
	];

	return (
		<>
			<SettingsDialogSubHeader style={{ marginTop: '20px' }}>On Startup</SettingsDialogSubHeader>

			<Field label={'Check for Millennium updates'}>
				<Toggle value={config.general.checkForMillenniumUpdates} onChange={(e) => handleChange('checkForMillenniumUpdates', e)} />
			</Field>

			<Field label={'Check for theme & plugin updates'} bottomSeparator="none">
				<Toggle value={config.general.checkForPluginAndThemeUpdates} onChange={(e) => handleChange('checkForPluginAndThemeUpdates', e)} />
			</Field>

			<SettingsDialogSubHeader style={{ marginTop: '20px' }}>Updates</SettingsDialogSubHeader>

			<Field label={'When an update for Millennium is available'} bottomSeparator="none">
				<Dropdown
					rgOptions={OnMillenniumUpdateOpts}
					selectedOption={OnMillenniumUpdateOpts.findIndex((opt) => opt.data === config.general.onMillenniumUpdate)}
					onChange={(e) => handleChange('onMillenniumUpdate', e.data)}
					contextMenuPositionOptions={{ bMatchWidth: false }}
					strDefaultLabel={'Automatically install'}
				/>
			</Field>

			<SettingsDialogSubHeader style={{ marginTop: '20px' }}>Notifications</SettingsDialogSubHeader>

			<Field label={'When a plugin or theme update is available'} bottomSeparator="none">
				<Toggle value={config.general.shouldShowThemePluginUpdateNotifications} onChange={(e) => handleChange('shouldShowThemePluginUpdateNotifications', e)} />
			</Field>

			<SettingsDialogSubHeader style={{ marginTop: '20px' }}>Themes</SettingsDialogSubHeader>

			<Field label={locale.themePanelInjectJavascript}>
				<Toggle value={config.general.injectJavascript} onChange={(e) => handleChange('injectJavascript', e)} />
			</Field>
			<Field label={locale.themePanelInjectCSS}>
				<Toggle value={config.general.injectCSS} onChange={(e) => handleChange('injectCSS', e)} />
			</Field>

			<RenderAccentColorPicker />
		</>
	);
};
