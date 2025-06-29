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

import { callable, DialogButton, Field, Menu, MenuItem, pluginSelf, showContextMenu, showModal } from '@steambrew/client';
import { ThemeItem } from '../../types';
import { Separator } from '../../components/SteamComponents';
import { RenderThemeEditor } from '../../components/ThemeEditor';
import { Utils } from '../../utils';
import { formatString, locale } from '../../../locales';
import { FaCheck, FaEllipsisH } from 'react-icons/fa';
import { SiKofi } from 'react-icons/si';
import { Component } from 'react';
import { PyUninstallTheme } from '../../utils/ffi';
import { IconButton } from '../../components/IconButton';

interface ThemeItemComponentProps {
	theme: ThemeItem;
	isLastItem: boolean;
	activeTheme?: string;
	onChangeTheme: (item: ThemeItem) => void;
	onUseDefault: () => void;
}

interface ThemeItemComponentState {
	shouldShowMore: boolean;
}

export enum UIReloadProps {
	None,
	Force,
	Prompt,
}

export const ChangeActiveTheme = async (themeName: string, reloadProps: UIReloadProps) => {
	await callable<[{ theme_name: string }]>('theme_config.change_theme')({ theme_name: themeName });

	return new Promise((resolve) => {
		switch (reloadProps) {
			case UIReloadProps.Force:
				SteamClient.Browser.RestartJSContext();
				resolve(true);
				break;
			case UIReloadProps.Prompt:
				Utils.PromptReload((hasClickedOk) => {
					SteamClient.Browser.RestartJSContext();
					resolve(hasClickedOk);
				});
				break;
			default:
				resolve(true);
				break;
		}
	});
};

export class ThemeItemComponent extends Component<ThemeItemComponentProps, ThemeItemComponentState> {
	constructor(props: ThemeItemComponentProps) {
		super(props);
		this.state = {
			shouldShowMore: false,
		};

		this.showCtxMenu = this.showCtxMenu.bind(this);
	}

	get isActive() {
		const { theme, activeTheme } = this.props;
		return theme?.data?.name === activeTheme || theme?.native === activeTheme;
	}

	get bIsThemeConfigurable() {
		const { theme } = this.props;
		return theme?.data?.Conditions !== undefined || theme?.data?.RootColors !== undefined;
	}

	openThemeSettings() {
		const { theme } = this.props;
		const isActive = this.isActive;

		function onClose() {
			if (!isActive || !pluginSelf.ConditionConfigHasChanged) return;

			/** After the new config is set, we need to reload the UI for changes to be reflected */
			Utils.PromptReload(() => SteamClient.Browser.RestartJSContext());
			pluginSelf.ConditionConfigHasChanged = false;
		}

		showModal(<RenderThemeEditor theme={theme} />, pluginSelf.mainWindow, {
			strTitle: theme?.data?.name,
			popupHeight: 675,
			popupWidth: 850,
			fnOnClose: onClose,
		});
	}

	openThemeFolder() {
		const { theme } = this.props;

		const themesPath = [pluginSelf.steamPath, 'steamui', 'skins', theme.native].join('/');
		Utils.BrowseLocalFolder(themesPath);
	}

	uninstallTheme() {
		const { theme } = this.props;

		PyUninstallTheme({ owner: theme.data.github.owner, repo: theme.data.github.repo_name }).then(() => {
			SteamClient.Browser.RestartJSContext();
		});
	}

	showCtxMenu(event: MouseEvent | GamepadEvent) {
		const { theme, onChangeTheme, onUseDefault } = this.props;
		const { shouldShowMore } = this.state;
		const isActive = this.isActive;

		console.log('Menu', Menu);

		showContextMenu(
			<Menu label={theme?.data?.name}>
				<MenuItem disabled tone="emphasis" bInteractableItem={false}>
					{theme?.data?.name} {theme?.data?.version && <>v{theme.data.version}</>}
				</MenuItem>
				<Separator />
				{isActive ? <MenuItem onSelected={onUseDefault}>Disable</MenuItem> : <MenuItem onSelected={() => onChangeTheme(theme)}>Set as active</MenuItem>}
				<MenuItem onSelected={this.openThemeSettings.bind(this)} disabled={!this.bIsThemeConfigurable}>
					Configure
				</MenuItem>
				<MenuItem onSelected={() => this.setState({ shouldShowMore: !shouldShowMore })}>{shouldShowMore ? 'Show less...' : 'Show more...'}</MenuItem>
				<MenuItem onSelected={this.openThemeFolder.bind(this)}>Browse local files</MenuItem>
				<MenuItem onSelected={this.uninstallTheme.bind(this)}>Uninstall</MenuItem>
			</Menu>,
			event.currentTarget ?? window,
		);
	}

	renderExpandableShowMore() {
		const { theme } = this.props;
		if (!theme?.data?.description && !theme?.data?.github?.owner) return null;

		return (
			<>
				{theme.data.description && <div className="MillenniumThemes_Description">{theme.data.description}</div>}
				{theme.data.github?.owner && (
					<a className="MillenniumThemes_Author" onClick={() => Utils.OpenUrl('https://github.com/' + theme.data.github.owner)}>
						{formatString(locale.strByAuthor, theme.data.github.owner)}
					</a>
				)}
			</>
		);
	}

	render() {
		const { theme, isLastItem } = this.props;
		const { shouldShowMore } = this.state;
		const isActive = this.isActive;

		return (
			<Field
				label={
					<div className="MillenniumThemes_ThemeLabel">
						{theme?.data?.name}
						{theme?.data?.version && <div className="MillenniumItem_Version">v{theme.data.version}</div>}
					</div>
				}
				padding="standard"
				bottomSeparator={isLastItem ? 'none' : 'standard'}
				className="MillenniumThemes_ThemeItem"
				{...(isActive && { icon: <FaCheck /> })}
				{...(shouldShowMore && { description: this.renderExpandableShowMore() })}
				data-theme-name={theme?.data?.name}
				data-theme-folder-name-on-disk={theme?.native}
			>
				{shouldShowMore && theme?.data?.funding?.kofi && (
					<IconButton onClick={() => Utils.OpenUrl('https://ko-fi.com/' + theme.data.funding.kofi)}>
						<SiKofi />
					</IconButton>
				)}
				<IconButton onClick={this.showCtxMenu}>
					<FaEllipsisH />
				</IconButton>
			</Field>
		);
	}
}
