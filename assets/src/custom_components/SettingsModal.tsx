import React, { useEffect, useMemo, useState } from 'react';
import { Classes, DialogBody, findModuleDetailsByExport, IconsModule, ModalPosition, pluginSelf, SidebarNavigation, type SidebarNavigationPage } from '@steambrew/client';
import { settingsClasses } from '../classes';
import { PluginViewModal } from '../tabs/Plugins';
import { ThemeViewModal } from '../tabs/Themes';
import { RenderUpdatesSettingsTab, UpdatesViewModal } from '../tabs/Updates';
import { locale } from '../locales';
import { LogsViewModal } from '../tabs/Logs';
import Styles from '../styles';
import { MillenniumIcons } from '../icons';
import ReactDOM, { createPortal } from 'react-dom';
import { GeneralViewModal } from '../tabs/general';

export class MillenniumSettings extends React.Component {
	render() {
		const className = `${settingsClasses.SettingsModal} ${settingsClasses.DesktopPopup} MillenniumSettings`;

		const pages: SidebarNavigationPage[] = [
			{
				visible: true,
				title: locale.settingsPanelGeneral,
				icon: <IconsModule.Settings />,
				content: (
					<DialogBody className={Classes.SettingsDialogBodyFade}>
						<GeneralViewModal />
					</DialogBody>
				),
			},
			{
				visible: true,
				title: locale.settingsPanelThemes,
				icon: <MillenniumIcons.Themes />,
				content: (
					<DialogBody className={Classes.SettingsDialogBodyFade}>
						<ThemeViewModal />
					</DialogBody>
				),
			},
			{
				visible: true,
				title: locale.settingsPanelPlugins,
				icon: <MillenniumIcons.Plugins />,
				content: (
					<DialogBody className={Classes.SettingsDialogBodyFade}>
						<PluginViewModal />
					</DialogBody>
				),
			},
			{
				visible: true,
				title: <RenderUpdatesSettingsTab />,
				icon: <IconsModule.Update />,
				content: (
					<DialogBody className={Classes.SettingsDialogBodyFade}>
						<UpdatesViewModal />
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
		];

		return (
			<>
				<div className="FullModalOverlay" style={{ display: 'none' }}>
					<div className="ModalOverlayContent ModalOverlayBackground"></div>
				</div>
				<ModalPosition>
					<Styles />
					{/* @ts-ignore */}
					<SidebarNavigation className={className} pages={pages} title={`Millennium ${pluginSelf.version}`} />
				</ModalPosition>
			</>
		);
	}
}
