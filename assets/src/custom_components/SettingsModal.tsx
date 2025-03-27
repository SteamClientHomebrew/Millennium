import React from 'react';
import {
	Classes,
	ClassModule,
	DialogBody,
	findClassModule,
	IconsModule,
	ModalPosition,
	pluginSelf,
	SidebarNavigation,
	type SidebarNavigationPage,
} from '@steambrew/client';
import { settingsClasses } from '../classes';
import * as CustomIcons from './CustomIcons';
import { PluginViewModal } from '../tabs/Plugins';
import { ThemeViewModal } from '../tabs/Themes';
import { UpdatesViewModal } from '../tabs/Updates';
import { locale } from '../locales';
import { BugReportViewModal } from '../tabs/BugReport';
import { LogsViewModal } from '../tabs/Logs';
import { SettingsViewModal } from '../tabs/Settings';

export class MillenniumSettings extends React.Component {
	render() {
		const className = `${settingsClasses.SettingsModal} ${settingsClasses.DesktopPopup} millennium-settings`;

		const pages: SidebarNavigationPage[] = [
			{
				visible: true,
				title: locale.settingsPanelThemes,
				icon: <CustomIcons.Themes />,
				content: (
					<DialogBody className={Classes.SettingsDialogBodyFade}>
						<ThemeViewModal />
					</DialogBody>
				),
			},
			{
				visible: true,
				title: locale.settingsPanelPlugins,
				icon: <CustomIcons.Plugins />,
				content: (
					<DialogBody className={Classes.SettingsDialogBodyFade}>
						<PluginViewModal />
					</DialogBody>
				),
			},
			{
				visible: true,
				title: locale.settingsPanelUpdates,
				icon: <IconsModule.Update />,
				content: (
					<DialogBody className={Classes.SettingsDialogBodyFade}>
						<UpdatesViewModal />
					</DialogBody>
				),
			},
			{
				visible: true,
				title: locale.settingsPanelBugReport,
				icon: <IconsModule.BugReport />,
				content: (
					<DialogBody className={Classes.SettingsDialogBodyFade}>
						<BugReportViewModal />
					</DialogBody>
				),
			},
			{
				visible: true,
				title: locale.settingsPanelLogs,
				icon: <IconsModule.TextCodeBlock />,
				content: (
					<DialogBody className={Classes.SettingsDialogBodyFade}>
						<LogsViewModal />
					</DialogBody>
				),
			},
			{
				visible: true,
				title: locale.settingsPanelSettings,
				icon: <IconsModule.Settings />,
				content: (
					<DialogBody className={Classes.SettingsDialogBodyFade}>
						<SettingsViewModal />
					</DialogBody>
				),
			},
		];

		const SettingsClasses = findClassModule((m) => m.SettingsTitleBar && m.SettingsModal) as ClassModule;

		return (
			<ModalPosition>
				<style>{`
                    .${SettingsClasses.SettingsModal}.${SettingsClasses.DesktopPopup} {
                        min-height: unset !important;
                    }
                    .ModalDialogPopup .PopupFullWindow .DialogContent_InnerWidth {
                        max-width: 620px;
                    }
                    input.colorPicker { margin-left: 10px !important; border: unset !important; min-width: 38px; width: 38px !important; height: 38px; !important; background: transparent; padding: unset !important; }
                `}</style>

				{/* @ts-ignore */}
				<SidebarNavigation className={className} pages={pages} title={`Millennium ${pluginSelf.version}`} />
			</ModalPosition>
		);
	}
}
