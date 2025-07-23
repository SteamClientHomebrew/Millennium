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

import { DialogButton, DialogControlsSection, IconsModule, joinClassNames, pluginSelf } from '@steambrew/client';
import { ThemeItem } from '../../types';
import { locale } from '../../../locales';
import { Placeholder } from '../../components/Placeholder';
import { PyFindAllThemes } from '../../utils/ffi';
import { Component } from 'react';
import { ChangeActiveTheme, ThemeItemComponent, UIReloadProps } from './ThemeComponent';
import { DialogControlSectionClass, settingsClasses } from '../../utils/classes';
import { showInstallThemeModal } from './ThemeInstallerModal';
import { FaFolderOpen, FaPaintRoller, FaStore } from 'react-icons/fa';
import { Utils } from '../../utils';
import { SettingsDialogSubHeader } from '../../components/SteamComponents';
import { MillenniumIcons } from '../../components/Icons';

const findAllThemes = async (): Promise<ThemeItem[]> => {
	return JSON.parse(await PyFindAllThemes());
};

interface ThemeViewModalState {
	themes?: ThemeItem[];
	active?: string;
}

export class ThemeViewModal extends Component<{}, ThemeViewModalState> {
	OpenPluginsFolder: any;
	constructor(props: {}) {
		super(props);
		this.state = {
			themes: undefined,
			active: undefined,
		};
	}

	componentDidMount() {
		const activeTheme: ThemeItem = pluginSelf.activeTheme;
		const active = pluginSelf.isDefaultTheme ? 'Default' : activeTheme?.data?.name ?? activeTheme?.native;

		this.setState({ active });
		this.FetchAllPlugins();
	}

	UseDefaultTheme = () => {
		/** Default theme object */
		this.ChangeActiveTheme({ native: 'default', data: null, failed: false });
	};

	ChangeActiveTheme = (item: ThemeItem) => {
		ChangeActiveTheme(item.native, UIReloadProps.Prompt).then((hasClickedOk) => {
			/** Reload the themes */
			!hasClickedOk && findAllThemes().then((themes) => this.setState({ themes }));
		});
	};

	IsActiveTheme = (theme: ThemeItem): boolean => {
		const { active } = this.state;
		return theme?.data?.name === active || theme?.native === active;
	};

	RenderThemeItem = (theme: ThemeItem, isLastItem: boolean, index: number) => {
		return (
			<ThemeItemComponent
				key={index}
				theme={theme}
				isLastItem={isLastItem}
				activeTheme={this.state.active}
				onChangeTheme={this.ChangeActiveTheme}
				onUseDefault={this.UseDefaultTheme}
				fetchThemes={this.FetchAllPlugins.bind(this)}
			/>
		);
	};

	FetchAllPlugins = () => {
		findAllThemes().then((themes) => {
			this.setState({ themes });
		});
	};

	OpenThemesFolder = () => {
		const themesPath = [pluginSelf.steamPath, 'steamui', 'skins'].join('/');
		Utils.BrowseLocalFolder(themesPath);
	};

	InstallPluginMenu = () => {
		showInstallThemeModal(this.FetchAllPlugins);
	};

	render() {
		if (pluginSelf.connectionFailed) {
			return (
				<Placeholder icon={<IconsModule.ExclamationPoint />} header={locale.errorFailedConnection} body={locale.errorFailedConnectionBody}>
					<DialogButton
						className={settingsClasses.SettingsDialogButton}
						onClick={() => SteamClient.System.OpenLocalDirectoryInSystemExplorer([pluginSelf.steamPath, 'ext', 'data', 'logs'].join('/'))}
					>
						{locale.errorFailedConnectionButton}
					</DialogButton>
				</Placeholder>
			);
		}

		/** Haven't received the themes yet from the backend */
		if (this.state.themes === undefined) {
			return null;
		}

		if (!this.state.themes || !this.state.themes.length) {
			return (
				<Placeholder icon={<FaPaintRoller className="SVGIcon_Button" />} header="No themes found" body="It appears you don't have any themes yet!">
					<DialogButton className={joinClassNames(settingsClasses.SettingsDialogButton, 'MillenniumPlaceholder_Button')} onClick={this.InstallPluginMenu.bind(this)}>
						<FaStore />
						{locale.optionInstallTheme}
					</DialogButton>
					<DialogButton className={joinClassNames(settingsClasses.SettingsDialogButton, 'MillenniumPlaceholder_Button')} onClick={this.OpenThemesFolder}>
						<FaFolderOpen />
						{locale.optionBrowseLocalFiles}
					</DialogButton>
				</Placeholder>
			);
		}

		const { themes } = this.state;
		return (
			<>
				<DialogControlsSection className="MillenniumButtonsSection">
					<DialogButton className={`MillenniumButton ${settingsClasses.SettingsDialogButton}`} onClick={this.InstallPluginMenu.bind(this)}>
						<FaStore />
						{locale.optionInstallTheme}
					</DialogButton>
					<DialogButton className={`MillenniumButton ${settingsClasses.SettingsDialogButton}`} onClick={this.OpenThemesFolder}>
						<FaFolderOpen />
						{locale.optionBrowseLocalFiles}
					</DialogButton>
				</DialogControlsSection>
				{themes?.map((theme, i) => this.RenderThemeItem(theme, i === themes.length - 1, i))}
			</>
		);
	}
}
