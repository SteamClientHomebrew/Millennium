import {
    Classes,
    DialogBody,
    DialogBodyText,
    DialogButton,
    DialogHeader,
    DialogSubHeader,
    Millennium,
    pluginSelf,
} from "@millennium/ui"
import { ThemeItem } from "../types"
import { CreatePopup } from "../components/CreatePopup"
import React from "react"
import { locale } from "../locales";
import { devClasses, settingsClasses } from "../classes";
import { FakeFriend } from "./FakeFriend";

class AboutThemeRenderer extends React.Component<any> {
    
    activeTheme: ThemeItem;

    constructor(props: any) {
        super(props)

        this.activeTheme = pluginSelf.activeTheme
    }

    RenderDeveloperProfile = () => {

        const OpenDeveloperProfile = () => {
            this.activeTheme?.data?.github?.owner 
            && SteamClient.System.OpenInSystemBrowser(`https://github.com/${this.activeTheme?.data?.github?.owner}/`)
        }

        return (
            <>
            <style>
                {`
                .${Classes.FakeFriend}.online:hover {
                    cursor: pointer !important;
                }
                
                .${Classes.avatarHolder}.avatarHolder.no-drag.Medium.online,
                .online.${devClasses.noContextMenu}.${devClasses.twoLine} {
                    pointer-events: none;
                }`}
            </style>

            <FakeFriend
                eStatus="online"
                strAvatarURL={
                    this.activeTheme?.data?.github?.owner
                        ? `https://github.com/${this.activeTheme?.data?.github?.owner}.png`
                        : "https://i.pinimg.com/736x/98/1d/6b/981d6b2e0ccb5e968a0618c8d47671da.jpg"
                }
                strGameName={`✅ ${locale.aboutThemeVerifiedDev}`}
                strPlayerName={
                    this.activeTheme?.data?.github?.owner ??
                        this.activeTheme?.data?.author ??
                        locale.aboutThemeAnonymous
                }
                onClick={OpenDeveloperProfile}
            />
            </>
        )
    }

    RenderDescription = () => {
        return (
            <>
                <DialogSubHeader className={settingsClasses.SettingsDialogSubHeader}>{locale.aboutThemeTitle}</DialogSubHeader>
                <DialogBodyText className={Classes.FriendsDescription}>{this.activeTheme?.data?.description ?? locale.itemNoDescription}</DialogBodyText>
            </>
        )
    }

    RenderInfoRow = () => {

        const themeOwner = this.activeTheme?.data?.github?.owner
        const themeRepo = this.activeTheme?.data?.github?.repo_name
        const kofiDonate = this.activeTheme?.data?.funding?.kofi

        const ShowSource = () => {
            SteamClient.System.OpenInSystemBrowser(`https://github.com/${themeOwner}/${themeRepo}`)
        }

        const OpenDonateDefault = () => {
            SteamClient.System.OpenInSystemBrowser(`https://ko-fi.com/${kofiDonate}`)
        }

        const ShowInFolder = () => {
            Millennium.callServerMethod("Millennium.steam_path")
            .then((result: any) => {
                pluginSelf.connectionFailed = false
                return result
            })
            .then((path: string) => {
                console.log(path)
                SteamClient.System.OpenLocalDirectoryInSystemExplorer(`${path}/steamui/skins/${this.activeTheme.native}`)
            })
        }

        const UninstallTheme = () => {
            Millennium.callServerMethod("uninstall_theme", {
                owner: this.activeTheme?.data?.github?.owner,
                repo: this.activeTheme?.data?.github?.repo_name
            })
            .then((result: any) => {
                pluginSelf.connectionFailed = false
                return result
            })
            .then((raw: string) => {

                const message = JSON.parse(raw)
                console.log(message)

                SteamClient.Browser.RestartJSContext()
            })
        }

        return (
            <>
                {themeOwner && themeRepo && <DialogButton style={{width: "unset"}} className={settingsClasses.SettingsDialogButton} onClick={ShowSource}>{locale.viewSourceCode}</DialogButton>}
                {/* {kofiDonate && <button type="button" style={{width: "unset"}} className={`${settingsClasses.SettingsDialogButton} DialogButton _DialogLayout Secondary Focusable`} onClick={OpenDonateDefault}>Donate</button>} */}

                <div className=".flex-btn-container" style={{display: "flex", gap: "5px"}}>
                    <DialogButton style={{width: "50%", }} className={settingsClasses.SettingsDialogButton} onClick={ShowInFolder}>{locale.showInFolder}</DialogButton>
                    <DialogButton style={{width: "50%"}} className={settingsClasses.SettingsDialogButton} onClick={UninstallTheme}>{locale.uninstall}</DialogButton> 
                </div>
            </>
        )
    }

    CreateModalBody = () => {
        return (
            <div className="ModalPosition" tabIndex={0}>
                <div className="ModalPosition_Content" style={{width: "100vw", height: "100vh"}}>
                    <div className="DialogContent _DialogLayout GenericConfirmDialog _DialogCenterVertically">
                        <div className="DialogContent_InnerWidth" style={{flex: "unset"}}>
                            <DialogHeader>{this.activeTheme?.data?.name ?? this.activeTheme?.native}</DialogHeader>
                            <DialogBody style={{flex: "unset"}}>
                                <this.RenderDeveloperProfile/>
                                <this.RenderDescription/>
                                <this.RenderInfoRow/>
                            </DialogBody>
                        </div>
                    </div>
                </div>
            </div>
        )
    }

    render() {
        return (
            <this.CreateModalBody/>
        )
    }
}


export const SetupAboutRenderer = (active: string) => {
    const params: any = {
        title: locale.aboutThemeTitle + " " + active,
        popup_class: "fullheight",
        body_class: "fullheight ModalDialogBody DesktopUI ",
        html_class: "client_chat_frame fullheight ModalDialogPopup ",
        eCreationFlags: 274,
        window_opener_id: 1,
        dimensions: {width: 450, height: 375},
        replace_existing_popup: false,
    }

    const popupWND = new CreatePopup(AboutThemeRenderer, "about_theme", params)
    popupWND.Show()
}