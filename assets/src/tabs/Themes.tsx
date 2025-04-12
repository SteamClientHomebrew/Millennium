import { Millennium, ConfirmModal, Dropdown, DialogButton, IconsModule, pluginSelf, Toggle, showModal, Field, callable } from '@steambrew/client';
import * as CustomIcons from '../custom_components/CustomIcons';
import { useEffect, useRef, useState } from 'react';
import { RenderThemeEditor } from '../custom_components/ThemeEditor';
import { ComboItem, ThemeItem } from '../types';
import { SetupAboutRenderer } from '../custom_components/AboutTheme';
import { locale } from '../locales';
import { ConnectionFailed } from '../custom_components/ConnectionFailed';
import { RenderAccentColorPicker } from './AccentColorPicker';
import ReactDOM from 'react-dom';

const Localize = (token: string): string =>
	// @ts-ignore
	LocalizationManager.LocalizeString(token);

/**
 * @note
 * There is a specific webpack module for that,
 * but it restarts Steam instead of reloading the UI.
 */
const PromptReload = (onOK: () => void) =>
	showModal(
		<ConfirmModal
			strTitle={Localize('#Settings_RestartRequired_Title')}
			strDescription={Localize('#Settings_RestartRequired_Description')}
			strOKButtonText={Localize('#Settings_RestartNow_ButtonText')}
			onOK={onOK}
		/>,
		pluginSelf.windows['Millennium'],
		{
			bNeverPopOut: false,
		},
	);

const ThemeSettings = (activeTheme: string) =>
	showModal(<RenderThemeEditor />, pluginSelf.windows['Millennium'], {
		strTitle: activeTheme + ' Settings',
		popupHeight: 675,
		popupWidth: 850,
		fnOnClose: () => {
			if (!pluginSelf.ConditionConfigHasChanged) {
				return;
			}

			PromptReload(() => {
				SteamClient.Browser.RestartJSContext();
			});
			pluginSelf.ConditionConfigHasChanged = false;
		},
	});

interface EditThemeProps {
	active: string;
}

/**
 * Display the edit theme button on a theme if applicable
 * @param active the common name of a theme
 * @returns react component
 */
const RenderEditTheme: React.FC<EditThemeProps> = ({ active }) => {
	const Theme = pluginSelf.activeTheme as ThemeItem;

	/** Current theme is not editable */
	if (pluginSelf?.isDefaultTheme || (Theme?.data?.Conditions === undefined && Theme?.data?.RootColors === undefined)) {
		return <></>;
	}

	return (
		<DialogButton onClick={() => ThemeSettings(active)} className="MillenniumIconButton _3epr8QYWw_FqFgMx38YEEm">
			<IconsModule.Settings height="16" />
		</DialogButton>
	);
};

const findAllThemes = async (): Promise<ComboItem[]> => {
	const activeTheme: ThemeItem = await Millennium.callServerMethod('cfg.get_active_theme').then((result: any) => {
		pluginSelf.connectionFailed = false;
		return result;
	});

	return new Promise<ComboItem[]>(async (resolve) => {
		let buffer: ComboItem[] = (
			JSON.parse(
				await Millennium.callServerMethod('find_all_themes').then((result: any) => {
					pluginSelf.connectionFailed = false;
					return result;
				}),
			) as ThemeItem[]
		)

			/** Prevent the selected theme from appearing in combo box */
			.filter((theme: ThemeItem) => (!pluginSelf.isDefaultTheme ? theme.native !== activeTheme.native : true))
			.map((theme: ThemeItem, index: number) => ({
				label: theme?.data?.name ?? theme.native,
				theme: theme,
				data: 'theme' + index,
			}));

		/** Add the default theme to list */
		!pluginSelf.isDefaultTheme && buffer.unshift({ label: '< Default >', data: 'default', theme: null });
		resolve(buffer);
	});
};

const ThemeViewModal: React.FC = () => {
	const [themes, setThemes] = useState<ComboItem[]>();
	const [active, setActive] = useState<string>();
	const [jsState, setJsState] = useState<boolean>(undefined);
	const [cssState, setCssState] = useState<boolean>(undefined);
	const [themeUsesAccentColor, setThemeUsesAccentColor] = useState<boolean>(undefined);

	useEffect(() => {
		const activeTheme: ThemeItem = pluginSelf.activeTheme;
		pluginSelf.isDefaultTheme ? setActive('Default') : setActive(activeTheme?.data?.name ?? activeTheme?.native);

		findAllThemes().then((result: ComboItem[]) => setThemes(result));

		setJsState(pluginSelf.scriptsAllowed);
		setCssState(pluginSelf.stylesAllowed);

		const bDoesUseAccentColor = callable<[]>('cfg.does_theme_use_accent_color');

		bDoesUseAccentColor().then((result: any) => {
			console.log('Does theme use accent color', result);
			setThemeUsesAccentColor(result);
		});
	}, []);

	const onScriptToggle = (enabled: boolean) => {
		setJsState(enabled);

		PromptReload(() => {
			Millennium.callServerMethod('cfg.set_config_keypair', { key: 'scripts', value: enabled })
				.then((result: any) => {
					pluginSelf.connectionFailed = false;
					return result;
				})
				.catch((_: any) => {
					console.error('Failed to update settings');
					pluginSelf.connectionFailed = true;
				});

			SteamClient.Browser.RestartJSContext();
		});
	};

	const onStyleToggle = (enabled: boolean) => {
		setCssState(enabled);

		PromptReload(() => {
			Millennium.callServerMethod('cfg.set_config_keypair', { key: 'styles', value: enabled })
				.then((result: any) => {
					pluginSelf.connectionFailed = false;
					return result;
				})
				.catch((_: any) => {
					console.error('Failed to update settings');
					pluginSelf.connectionFailed = true;
				});

			SteamClient.Browser.RestartJSContext();
		});
	};

	const updateThemeCallback = (item: ComboItem) => {
		const themeName = item.data === 'default' ? '__default__' : item.theme.native;

		Millennium.callServerMethod('cfg.change_theme', { theme_name: themeName }).then((result: any) => {
			pluginSelf.connectionFailed = false;
			return result;
		});
		findAllThemes().then((result: ComboItem[]) => setThemes(result));

		PromptReload(() => {
			SteamClient.Browser.RestartJSContext();
		});
	};

	if (pluginSelf.connectionFailed) {
		return <ConnectionFailed />;
	}

	const OpenThemesFolder = () => {
		const themesPath = [pluginSelf.steamPath, 'steamui', 'skins'].join('/');
		SteamClient.System.OpenLocalDirectoryInSystemExplorer(themesPath);
	};

	const GetMoreThemes = () => {
		SteamClient.System.OpenInSystemBrowser('https://steambrew.app/themes');
	};

	const [isHoveringThemeDropdown, setIsHoveringThemeDropdown] = useState(false);

	const PortalComponent = () => {
		const elementRef = useRef(null);
		const [position, setPosition] = useState({ top: 0, left: '0px' });

		useEffect(() => {
			const element = pluginSelf.windows['Millennium'].document.querySelector('.DialogDropDown');
			if (!element) return;

			const rect = element.getBoundingClientRect();

			if (elementRef.current) {
				const refRect = elementRef.current.getBoundingClientRect();
				setPosition({
					top: rect.top + rect.height + 10,
					left: rect.right - refRect.width + 'px',
				});
			}
		}, []);

		return ReactDOM.createPortal(
			<div ref={elementRef} className="_3vg1vYU7iTWqONciv9cuJN _1Ye_0niF2UqB8uQTbm8B6B" style={{ top: position.top, left: position.left }}>
				<div className="_2FxbHJzYoH024ko7zqcJOf" style={{ maxWidth: '50vw' }}>
					Millennium can't find any themes in your skins folder. If you have themes installed that aren't showing, verify you've extracted
					them properly. The skin.json file should be in the root of the theme folder.
					<br />
					<br />
					Example:<br></br> <code>Steam/steamui/skins/SkinName/skin.json</code>
					<br></br> instead of<br></br> <code>Steam/steamui/skins/SkinName/SkinName/skin.json</code>
				</div>
			</div>,
			pluginSelf.windows['Millennium'].document.body,
		);
	};

	return (
		<>
			<Field
				label={locale.themePanelClientTheme}
				description={
					<>
						{locale.themePanelThemeTooltip}
						{'. '}
						<a href="#" onClick={GetMoreThemes}>
							{locale.themePanelGetMoreThemes}
						</a>
					</>
				}
			>
				<RenderEditTheme active={active} />

				{!pluginSelf.isDefaultTheme && (
					<DialogButton onClick={() => SetupAboutRenderer(active)} className="MillenniumIconButton _3epr8QYWw_FqFgMx38YEEm">
						<IconsModule.Information height="16" />
					</DialogButton>
				)}

				<DialogButton onClick={OpenThemesFolder} className="MillenniumIconButton _3epr8QYWw_FqFgMx38YEEm">
					<CustomIcons.Folder />
				</DialogButton>

				<div onMouseEnter={() => setIsHoveringThemeDropdown(true)} onMouseLeave={() => setIsHoveringThemeDropdown(false)}>
					<Dropdown
						onMenuOpened={async () => await findAllThemes().then((result: ComboItem[]) => setThemes(result))}
						contextMenuPositionOptions={{ bMatchWidth: false }}
						rgOptions={themes as any}
						selectedOption={1}
						strDefaultLabel={active}
						onChange={updateThemeCallback as any}
						disabled={themes?.length === 0}
					/>
				</div>
				{isHoveringThemeDropdown && themes?.length === 0 && <PortalComponent />}
			</Field>

			{/* Render inject javascript checkbox */}
			<Field label={locale.themePanelInjectJavascript} description={locale.themePanelInjectJavascriptToolTip}>
				{jsState !== undefined && <Toggle value={jsState} onChange={onScriptToggle} />}
			</Field>

			{/* Render inject stylesheet checkbox */}
			<Field label={locale.themePanelInjectCSS} description={locale.themePanelInjectCSSToolTip}>
				{cssState !== undefined && <Toggle value={cssState} onChange={onStyleToggle} />}
			</Field>

			<RenderAccentColorPicker currentThemeUsesAccentColor={themeUsesAccentColor} />
		</>
	);
};

export { ThemeViewModal };
