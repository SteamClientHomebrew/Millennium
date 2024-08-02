import { 
    Millennium, 
    ConfirmModal,
    Dropdown, 
    DialogHeader, 
    DialogBody, 
    DialogButton,
    classMap, 
    IconsModule, 
    pluginSelf, 
    Toggle, 
    showModal, 
} from '@millennium/ui'
import * as CustomIcons from '../custom_components/CustomIcons'
import { Field } from '../custom_components/Field'

import { useEffect, useState } from 'react'
import { RenderThemeEditor } from '../custom_components/ThemeEditor'
import { ComboItem, ThemeItem } from '../types'
import { SetupAboutRenderer } from '../custom_components/AboutTheme'
import { locale } from '../locales'
import { ConnectionFailed } from '../custom_components/ConnectionFailed'

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
			strTitle={Localize("#Settings_RestartRequired_Title")}
			strDescription={Localize("#Settings_RestartRequired_Description")}
            strOKButtonText={Localize("#Settings_RestartNow_ButtonText")}
			onOK={onOK}
		/>,
		pluginSelf.settingsWnd,
		{
			bNeverPopOut: true,
		},
	);

const ThemeSettings = (activeTheme: string) =>
    showModal(<RenderThemeEditor />, pluginSelf.settingsWnd, {
        strTitle: "Editing " + activeTheme,
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
    active: string
}

/**
 * Display the edit theme button on a theme if applicable
 * @param active the common name of a theme
 * @returns react component
 */
const RenderEditTheme: React.FC<EditThemeProps> = ({ active }) => {

    const Theme = (pluginSelf.activeTheme as ThemeItem)

    /** Current theme is not editable */
    if (pluginSelf?.isDefaultTheme || (Theme?.data?.Conditions === undefined && Theme?.data?.RootColors === undefined)) {
        return (<></>)
    }

    return (
        <DialogButton 
            onClick={() => ThemeSettings(active)}
            style={{margin: "0", padding: "0px 10px", marginRight: "10px"}} 
            className="_3epr8QYWw_FqFgMx38YEEm millenniumIconButton" 
        >
            <IconsModule.Settings height="16" />
        </DialogButton>
    )
}

const findAllThemes = async (): Promise<ComboItem[]> => {
    
    const activeTheme: ThemeItem = await Millennium.callServerMethod("cfg.get_active_theme")
    .then((result: any) => {
        pluginSelf.connectionFailed = false
        return result
    })

    return new Promise<ComboItem[]>(async (resolve) => {
        let buffer: ComboItem[] = (JSON.parse(await Millennium.callServerMethod("find_all_themes")
            .then((result: any) => {
                pluginSelf.connectionFailed = false
                return result
            })) as ThemeItem[])

            /** Prevent the selected theme from appearing in combo box */
            .filter((theme: ThemeItem) => !pluginSelf.isDefaultTheme ? theme.native !== activeTheme.native : true)
            .map((theme: ThemeItem, index: number) => ({ 

                label: theme?.data?.name ?? theme.native, theme: theme, data: "theme" + index 
            }))


        /** Add the default theme to list */
        !pluginSelf.isDefaultTheme && buffer.unshift({ label: "< Default >", data: "default", theme: null })
        resolve(buffer)
    })
}

const ThemeViewModal: React.FC = () => {

    const [themes, setThemes] = useState<ComboItem[]>()
    const [active, setActive] = useState<string>()
    const [jsState, setJsState] = useState<boolean>(undefined)
    const [cssState, setCssState] = useState<boolean>(undefined)

    useEffect(() => {

        const activeTheme: ThemeItem = pluginSelf.activeTheme
        pluginSelf.isDefaultTheme ? setActive("Default") : setActive(activeTheme?.data?.name ?? activeTheme?.native)

        findAllThemes().then((result: ComboItem[]) => setThemes(result))

        setJsState(pluginSelf.scriptsAllowed)
        setCssState(pluginSelf.stylesAllowed)
    }, [])

    const onScriptToggle = (enabled: boolean) => {
        setJsState(enabled)

        PromptReload(() => {
                Millennium.callServerMethod("cfg.set_config_keypair", {key: "scripts", value: enabled})
                .then((result: any) => {
                    pluginSelf.connectionFailed = false
                    return result
                })
                .catch((_: any) => {
                    console.error("Failed to update settings")
                    pluginSelf.connectionFailed = true
                })

                SteamClient.Browser.RestartJSContext()
        })
    }

    const onStyleToggle = (enabled: boolean) => {
        setCssState(enabled)

        PromptReload(() => {
            Millennium.callServerMethod("cfg.set_config_keypair", {key: "styles", value: enabled})
            .then((result: any) => {
                pluginSelf.connectionFailed = false
                return result
            })
            .catch((_: any) => {
                console.error("Failed to update settings")
                pluginSelf.connectionFailed = true
            })

            SteamClient.Browser.RestartJSContext()
        })
    }

    const updateThemeCallback = (item: ComboItem) => {
        const themeName = item.data === "default" ? "__default__" : item.theme.native;

        Millennium.callServerMethod("cfg.change_theme", {theme_name: themeName}).then((result: any) => {
            pluginSelf.connectionFailed = false
            return result
        });
        findAllThemes().then((result: ComboItem[]) => setThemes(result))

        PromptReload(() => {
            SteamClient.Browser.RestartJSContext();
        });
    }

	if (pluginSelf.connectionFailed) {
		return <ConnectionFailed/>
	}

    const OpenThemesFolder = () => {
        const themesPath = [pluginSelf.steamPath, "steamui", "skins"].join("/");
        SteamClient.System.OpenLocalDirectoryInSystemExplorer(themesPath);
    }

    const GetMoreThemes = () => {
        SteamClient.System.OpenInSystemBrowser("https://steambrew.app/themes");
    }

    return (
        <>
            <style>{`.DialogDropDown._DialogInputContainer.Panel.Focusable { min-width: max-content !important; }`}</style>

            <DialogHeader>{locale.settingsPanelThemes}</DialogHeader>
            <DialogBody className={classMap.SettingsDialogBodyFade}>
                <Field
                    label={locale.themePanelClientTheme}
                    description={
                        <>
                            {locale.themePanelThemeTooltip}
                            {". "}
                            <a href="#" onClick={GetMoreThemes}>
                                {locale.themePanelGetMoreThemes}
                            </a>
                        </>   
                    }       
                >
                    <RenderEditTheme active={active} />

                    {!pluginSelf.isDefaultTheme && (
                        <DialogButton
                            onClick={() => SetupAboutRenderer(active)}
                            style={{ margin: "0", padding: "0px 10px", marginRight: "10px" }}
                            className="_3epr8QYWw_FqFgMx38YEEm millenniumIconButton"
                        >
                            <IconsModule.Information height="16" />
                        </DialogButton>
                    )}
        
                    <DialogButton
                        onClick={OpenThemesFolder}
                        style={{ margin: "0", padding: "0px 10px", marginRight: "10px" }}
                        className="_3epr8QYWw_FqFgMx38YEEm millenniumIconButton"
                    >
                        <CustomIcons.Folder />
                    </DialogButton>
        
                    <Dropdown
                        contextMenuPositionOptions={{ bMatchWidth: false }}
                        rgOptions={themes as any}
                        selectedOption={1}
                        strDefaultLabel={active}
                        onChange={updateThemeCallback as any}
                    />
                </Field>

                <Field
                    label={locale.themePanelInjectJavascript}
                    description={locale.themePanelInjectCSSToolTip}
                >
                    {jsState !== undefined && (
                        <Toggle value={jsState} onChange={onScriptToggle} />
                    )}
                </Field>

                <Field
                    label={locale.themePanelInjectCSS}
                    description={locale.themePanelInjectCSSToolTip}
                >
                    {cssState !== undefined && (
                        <Toggle value={cssState} onChange={onStyleToggle} />
                    )}
                </Field>
            </DialogBody>
        </>
    )
}

export { ThemeViewModal }