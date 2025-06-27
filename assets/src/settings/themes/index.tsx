import { DialogButton, DialogControlsSection, pluginSelf } from '@steambrew/client';
import { ThemeItem } from '../../types';
import { locale } from '../../../locales';
import { Placeholder } from '../../components/Placeholder';
import { PyFindAllThemes } from '../../utils/ffi';
import { Component } from 'react';
import { ChangeActiveTheme, ThemeItemComponent, UIReloadProps } from './ThemeComponent';
import { DialogControlSectionClass, settingsClasses } from '../../utils/classes';
import { showInstallThemeModal } from './ThemeInstallerModal';
import { FaFolderOpen, FaStore } from 'react-icons/fa';
import { Utils } from '../../utils';
import { SettingsDialogSubHeader } from '../../components/SteamComponents';

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
				<Placeholder header={locale.errorFailedConnection} body={locale.errorFailedConnectionBody}>
					<DialogButton onClick={() => SteamClient.System.OpenLocalDirectoryInSystemExplorer([pluginSelf.steamPath, 'ext', 'data', 'logs'].join('/'))}>
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
				<Placeholder header="No themes found" body="It appears you don't have any themes yet!">
					<DialogButton onClick={showInstallThemeModal}>Install a theme</DialogButton>
				</Placeholder>
			);
		}

		const { themes } = this.state;
		return (
			<>
				<DialogControlsSection className="MillenniumButtonsSection">
					<DialogButton className={`MillenniumButton ${settingsClasses.SettingsDialogButton}`} onClick={showInstallThemeModal}>
						<FaStore />
						{locale.optionInstallTheme}
					</DialogButton>
					<DialogButton className={`MillenniumButton ${settingsClasses.SettingsDialogButton}`} onClick={this.openThemesFolder}>
						<FaFolderOpen />
						{locale.optionBrowseLocalFiles}
					</DialogButton>
				</DialogControlsSection>
				{themes?.map((theme, i) => this.renderThemeItem(theme, i === themes.length - 1, i))}
			</>
		);
	}
}
