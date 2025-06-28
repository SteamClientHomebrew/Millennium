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

import { DialogButton, pluginSelf } from '@steambrew/client';
import { ThemeItem } from '../../types';
import { locale } from '../../../locales';
import { ErrorModal } from '../../components/ErrorModal';
import { PyFindAllThemes } from '../../utils/ffi';
import { Component } from 'react';
import { ChangeActiveTheme, ThemeItemComponent, UIReloadProps } from './ThemeComponent';
import { DialogControlSectionClass, settingsClasses } from '../../utils/classes';
import { showInstallThemeModal } from './ThemeInstallerModal';
import { FaFolderOpen, FaStore } from 'react-icons/fa';
import { Utils } from '../../utils';

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

		findAllThemes().then((themes) => {
			this.setState({ themes });
		});
	}

	useDefaultTheme = () => {
		/** Default theme object */
		this.changeActiveTheme({ native: 'default', data: null, failed: false });
	};

	changeActiveTheme = (item: ThemeItem) => {
		ChangeActiveTheme(item.native, UIReloadProps.Prompt).then((hasClickedOk) => {
			/** Reload the themes */
			!hasClickedOk && findAllThemes().then((themes) => this.setState({ themes }));
		});
	};

	isActiveTheme = (theme: ThemeItem): boolean => {
		const { active } = this.state;
		return theme?.data?.name === active || theme?.native === active;
	};

	renderThemeItem = (theme: ThemeItem, isLastItem: boolean, index: number) => {
		return (
			<ThemeItemComponent
				key={index}
				theme={theme}
				isLastItem={isLastItem}
				activeTheme={this.state.active}
				onChangeTheme={this.changeActiveTheme}
				onUseDefault={this.useDefaultTheme}
			/>
		);
	};

	openThemesFolder = () => {
		const themesPath = [pluginSelf.steamPath, 'steamui', 'skins'].join('/');
		Utils.BrowseLocalFolder(themesPath);
	};

	render() {
		if (pluginSelf.connectionFailed) {
			return (
				<ErrorModal
					header={locale.errorFailedConnection}
					body={locale.errorFailedConnectionBody}
					options={{
						buttonText: locale.errorFailedConnectionButton,
						onClick: () => SteamClient.System.OpenLocalDirectoryInSystemExplorer([pluginSelf.steamPath, 'ext', 'data', 'logs'].join('/')),
					}}
				/>
			);
		}

		/** Haven't received the themes yet from the backend */
		if (this.state.themes === undefined) {
			return null;
		}

		if (!this.state.themes || !this.state.themes.length) {
			return (
				<ErrorModal
					header={'No Themes Found.'}
					body={"It appears you don't have any themes yet!"}
					showIcon={false}
					options={{
						buttonText: 'Install a Theme',
						onClick: showInstallThemeModal,
					}}
				/>
			);
		}

		const { themes } = this.state;
		return (
			<>
				<div
					className={`DialogControlsSection ${DialogControlSectionClass} MillenniumPluginsDialogControlsSection`}
					style={{ marginBottom: '10px', marginTop: '10px' }}
				>
					<DialogButton className={`MillenniumSpanningIconButton ${settingsClasses.SettingsDialogButton}`} onClick={showInstallThemeModal}>
						<FaStore />
						{locale.optionInstallTheme}
					</DialogButton>
					<DialogButton className={`MillenniumSpanningIconButton ${settingsClasses.SettingsDialogButton}`} onClick={this.openThemesFolder}>
						<FaFolderOpen />
						{locale.optionBrowseLocalFiles}
					</DialogButton>
				</div>
				{themes?.map((theme, i) => this.renderThemeItem(theme, i === themes.length - 1, i))}
			</>
		);
	}
}
