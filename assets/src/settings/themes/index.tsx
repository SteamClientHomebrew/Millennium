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

		const { themes } = this.state;
		return (
			<>
				<div
					className={`DialogControlsSection ${DialogControlSectionClass} MillenniumPluginsDialogControlsSection`}
					style={{ marginBottom: '10px', marginTop: '10px' }}
				>
					<DialogButton className={`MillenniumSpanningIconButton ${settingsClasses.SettingsDialogButton}`} onClick={showInstallThemeModal}>
						<FaStore />
						Install a theme
					</DialogButton>
					<DialogButton className={`MillenniumSpanningIconButton ${settingsClasses.SettingsDialogButton}`} onClick={() => {}}>
						<FaFolderOpen />
						Browse local files
					</DialogButton>
				</div>
				{themes?.map((theme, i) => this.renderThemeItem(theme, i === themes.length - 1, i))}
			</>
		);
	}
}
