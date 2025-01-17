import Markdown from 'markdown-to-jsx'
import { callable, ConfirmModal, Field, ModalPosition, pluginSelf, showModal, ShowModalResult, Toggle } from "@steambrew/client";
import { Separator } from "./PageList";
import { useState } from "react";
import { UpdaterOptionProps } from '../types';

/** Some helpful styles so the release info is so large */
const CSSStyles = `
.MillenniumReleaseMarkdown h2 {
    font-size: 16px;
}

.MillenniumReleaseMarkdown h3 {
    font-size: 15px;
}

.MillenniumReleaseMarkdown li {
    font-size: 14px;
}

.updateInfoTidbit {
    font-size: 13px;
}

button.DialogButton._DialogLayout {
    width: fit-content !important;
    max-width: fit-content !important;
}

.DialogBodyText {
    overflow-y: scroll !important;
}

.DesktopUI .DialogFooter {
    flex-direction: column !important;
}
`

/**
 * Backend callable functions defined in %root%/assets/core/*
 */
const UpdateMillennium = callable<[{ downloadUrl: string }], void>("MillenniumUpdater.queue_update");
const SetUserWantsUpdates = callable<[{ wantsUpdates: boolean }], void>("MillenniumUpdater.set_user_wants_updates");
const SetUserWantsNotifications = callable<[{ wantsNotify: boolean }], void>("MillenniumUpdater.set_user_wants_update_notify");

const ContainerURLStyles = {
    display: "flex",
    justifyContent: "flex-end",
    fontSize: "13px",
    marginBottom: "10px"
};

const LOG_UPDATE_INFO = (...params: any[]) => {
    console.log(
        `%c[Millennium Updater]%c`,
        'background: lightblue; color: black;',
        'background: inherit; color: inherit;',
        ...params
    )
};

const FormatBytes = (bytes: number, decimals = 2) => {
    if (bytes === 0) return '0 Bytes';

    const k = 1024;
    const dm = decimals < 0 ? 0 : decimals;
    const sizes = ['Bytes', 'KB', 'MB', 'GB', 'TB', 'PB', 'EB', 'ZB', 'YB'];

    const i = Math.floor(Math.log(bytes) / Math.log(k));
    return parseFloat((bytes / Math.pow(k, i)).toFixed(dm)) + ' ' + sizes[i];
}

const DateToLocalTime = (date: string) => {
    return new Date(date).toLocaleString()
}

const StartUpdateProcess = (updateNow: boolean, downloadUrl: string) => {
    UpdateMillennium({ downloadUrl }).then(() => {
        /** Restart the steam client */
        updateNow && SteamClient.User.StartRestart(false)
    })
}

const UpdateAvailablePopup = ({ props, targetAsset }: { props: any, targetAsset: any }) => {
    const [doNotShowAgain, setDoNotShowAgain] = useState(false);
    /** The markdown parser by default makes anchors redirect the current view port instead of loading it in a new window (i.e) an external browser */
    const MakeAnchorExternalLink = ({ children, ...props }: { children: any, props: any }) => <a target="_blank" {...props}>{children}</a>

    const OnDontShowAgainChange = (newValue: boolean) => {
        if (!newValue) {
            showModal(<ConfirmModal
                strTitle="Re-enable Updates?"
                strDescription="Do you want to re-enable update checks? You can always change this in Millennium Settings later."
                bHideCloseIcon={true}
                strOKButtonText="Yes"
                strCancelButtonText="No"
                onOK={() => {
                    /** 
                     * Re-enable updates and notifications if the user unchecks 
                     * the "Don't show this again" checkbox
                     */
                    SetUserWantsUpdates({ wantsUpdates: true });
                    SetUserWantsNotifications({ wantsNotify: true });

                    LOG_UPDATE_INFO("User has re-enabled updates and notifications")
                    setDoNotShowAgain(newValue)
                }}

            />, pluginSelf.millenniumUpdaterWindow, {});
            return
        }

        /** Disable update checking, and the ability to update as a whole. */
        const DisableUpdates = () => {
            SetUserWantsUpdates({ wantsUpdates: false });
            setDoNotShowAgain(newValue)

            LOG_UPDATE_INFO("User has disabled updates")
        }

        /** Disable update notifications, in this scenario, if update checking is on, updates will be automatically received. */
        const DisableNotifications = () => {
            SetUserWantsNotifications({ wantsNotify: false });
            setDoNotShowAgain(newValue)

            LOG_UPDATE_INFO("User has disabled notifications")
        }

        showModal(
            <ConfirmModal
                strTitle="One Sec!"
                strDescription="Do you want to disable update checks entirely, or just disable the update these notifications? You can always change this in Millennium Settings later."
                strOKButtonText="Disable Updates"
                strMiddleButtonText="Just Notifications"
                onOK={DisableUpdates}
                onMiddleButton={DisableNotifications}
                bHideCloseIcon={true}
            />,
            pluginSelf.millenniumUpdaterWindow,
            {}
        );
    }

    const ShowDownloadInformation = async () => {
        showModal(
            <ModalPosition>
                <style>{CSSStyles}</style>
                <div className="DialogContent _DialogLayout GenericConfirmDialog _DialogCenterVertically">
                    <div className="DialogContent_InnerWidth">
                        <form>
                            <div className="DialogHeader">Download Information</div>
                            <div className="DialogBody Panel">
                                <div className="DialogBodyText">
                                    <p className="updateInfoTidbit">
                                        <b>Published By</b>
                                        <div className="publisherInformation" style={{ display: "flex", gap: "10px" }}>
                                            <img src={props.author.avatar_url} alt="Author Avatar" style={{ width: "20px", height: "20px" }} />
                                            <div className="publisherInfo">
                                                {props.author.login}
                                            </div>
                                        </div>
                                        <br />
                                        <b>Version:</b> {props.tag_name} <br />
                                        <b>Release Date:</b> {DateToLocalTime(props.published_at)} <br />
                                        <b>Download Size:</b> {FormatBytes(targetAsset.size)} <br />
                                        <b>Download Link:</b> <a href={props.html_url} target="_blank">{targetAsset.name}</a>
                                    </p>
                                </div>
                            </div>
                        </form>
                    </div>
                </div >
            </ModalPosition >,
            pluginSelf.millenniumUpdaterWindow,
            {
                strTitle: "Download Information",
                popupHeight: 300,
                popupWidth: 450,
            }
        )
    }

    return (
        <ModalPosition>
            <style> {CSSStyles} </style>
            <div className="DialogContent _DialogLayout GenericConfirmDialog _DialogCenterVertically">
                <div className="DialogContent_InnerWidth">
                    <form>
                        <div className="DialogHeader">Update Available! 💫 </div>
                        <div className="DialogBody Panel">
                            <div className="DialogBodyText">
                                <p className="updateInfoTidbit">
                                    An update is available for Millennium! We're showing you this message because you've opted in to receive updates.
                                    If you no longer want to receive these messages, you can turn on automatic updates, or you can disable updates entirely from Millennium Settings.
                                </p>
                                <Separator />
                                <Markdown options={{ overrides: { a: { component: MakeAnchorExternalLink } } }} className="MillenniumReleaseMarkdown">
                                    {props.body}
                                </Markdown>
                            </div>
                            <div className="DialogFooter">
                                <div className="UpdateLinksContainer" style={{ display: "flex", gap: "10px", justifyContent: "flex-end" }}>
                                    <a className="viewDiffLink" href={`https://github.com/shdwmtr/millennium/compare/${pluginSelf.version}...v2.17.2`} target="_blank" style={ContainerURLStyles} >
                                        View whole diff in browser
                                    </a>
                                    <a className="viewDownloadInfo" onClick={() => ShowDownloadInformation()} target="_blank" style={ContainerURLStyles} >
                                        View Download Information
                                    </a>
                                </div>
                                <div className="DialogTwoColLayout _DialogColLayout Panel">
                                    <Field label={"Don't show this again"} bottomSeparator="none">
                                        <Toggle value={doNotShowAgain} onChange={OnDontShowAgainChange} />
                                    </Field>
                                    <button
                                        type="submit" className="DialogButton _DialogLayout Primary Focusable" tabIndex={0}
                                        onClick={() => StartUpdateProcess(false, targetAsset.browser_download_url)} >
                                        Update Next Startup
                                    </button>
                                    <button
                                        type="button" className="DialogButton _DialogLayout Secondary Focusable" tabIndex={0}
                                        onClick={() => { pluginSelf?.millenniumUpdaterWindow?.SteamClient?.Window?.Close() }} >
                                        I'll pass
                                    </button>
                                </div>
                            </div>
                        </div>
                    </form>
                </div>
            </div >
        </ModalPosition >
    )
}

export const PromptSelectUpdaterOptions = () => {

    /** Handle to the security window */
    let SecurityModalWindow: ShowModalResult;

    const RenderOptionSelector = () => {

        const [wantsUpdates, setWantsUpdates] = useState(pluginSelf.wantsMillenniumUpdates == UpdaterOptionProps.YES);
        const [wantsNotify, setWantsNotify] = useState(pluginSelf.wantsMillenniumUpdateNotifications == UpdaterOptionProps.YES);

        const OnUpdateChange = (newValue: boolean) => {
            setWantsUpdates(newValue)
        }

        const OnNotifyChange = (newValue: boolean) => {
            setWantsNotify(newValue)
        }

        return (
            <ModalPosition>
                <style> {CSSStyles} </style>
                <div className="DialogContent _DialogLayout GenericConfirmDialog _DialogCenterVertically">
                    <div className="DialogContent_InnerWidth">
                        <form>
                            <div className="DialogHeader">Millennium</div>
                            <div className="DialogBody Panel">
                                <div className="DialogBodyText" style={{ marginBottom: "unset" }}>
                                    <p className="updateInfoTidbit">
                                        We're deciding to update our security protocols to better benefit you, and the community as a whole.
                                        As of 1/16/2025, we've decided to implement measures to explicity ask you if you want to receive updates from Millennium.
                                        <br></br>
                                        <br></br>
                                        <b>(This does NOT include theme and plugin updates, which are handled separately, are we're never automatic)</b>
                                    </p>
                                    <Separator />
                                    <Field label="Do you want to receive updates from Millennium?" bottomSeparator="none">
                                        <Toggle value={wantsUpdates} onChange={OnUpdateChange} />
                                    </Field>
                                    <Field label="Do you want to receive notifications for updates? (similar to this popup)" bottomSeparator="none">
                                        <Toggle value={wantsNotify} onChange={OnNotifyChange} />
                                    </Field>
                                </div>
                                <div className="DialogFooter">
                                    <div className="DialogTwoColLayout _DialogColLayout Panel" style={{ flexDirection: "row", justifyContent: "space-between" }}>
                                        <p className='updateInfoTidbit' style={{ lineHeight: "7px" }}>You can change these settings later in Millennium Settings</p>
                                        <button
                                            type="submit"
                                            className="DialogButton _DialogLayout Primary Focusable"
                                            tabIndex={0}
                                            onClick={() => {
                                                SetUserWantsUpdates({ wantsUpdates: wantsUpdates });
                                                SetUserWantsNotifications({ wantsNotify: wantsNotify });
                                                SecurityModalWindow?.Close()
                                            }}
                                        >
                                            Continue
                                        </button>
                                    </div>
                                </div>
                            </div>
                        </form>
                    </div>
                </div >
            </ModalPosition>
        )
    }
    SecurityModalWindow = showModal(<RenderOptionSelector />, pluginSelf.mainWindow, {
        strTitle: "Select Update Options",
        popupHeight: 400,
        popupWidth: 575,
    })
}

export const ShowUpdaterModal = async () => {

    /** If the user hasn't specified their update options, prompt them */
    if (pluginSelf.wantsMillenniumUpdateNotifications == UpdaterOptionProps.UNSET || pluginSelf.wantsMillenniumUpdates == UpdaterOptionProps.UNSET) {
        LOG_UPDATE_INFO("User hasn't specified update options yet, prompting...")
        PromptSelectUpdaterOptions()
        return;
    }

    /** If the user doesn't want updates, cancel */
    if (pluginSelf.wantsMillenniumUpdates == UpdaterOptionProps.NO) {
        LOG_UPDATE_INFO("User has disabled updates, skipping...")
        return;
    }

    /** Pull updates from the backend in %root%/assets/core/updater/millennium.py */
    const GetAvailableUpdates = callable<[], any>("MillenniumUpdater.has_any_updates");
    const updates = JSON.parse(await GetAvailableUpdates());

    /** If there are no updates, cancel */
    // if (updates.hasUpdate === false) {
    //     LOG_UPDATE_INFO("No updates available, skipping...")
    //     return;
    // }

    const GetOSDownloadInformation = async () => {
        switch (await SteamClient.System.GetOSType()) {
            case 20: return (updates.newVersion.assets.find((asset: any) => asset.name === `millennium-${updates.newVersion.tag_name}-windows-x86_64.zip`))
            default: return (updates.newVersion.assets.find((asset: any) => asset.name === `millennium-${updates.newVersion.tag_name}-linux-x86_64.tar.gz`))
        }
    }

    const targetAsset = await GetOSDownloadInformation();

    if (pluginSelf.wantsMillenniumUpdateNotifications == UpdaterOptionProps.NO) {
        // Start the update process without showing the modal; as they want updates, but not notifications
        LOG_UPDATE_INFO("User has disabled update notifications, but updates are enabled. Starting update process...")
        StartUpdateProcess(false, targetAsset.browser_download_url)
        return;
    }

    LOG_UPDATE_INFO("Updates available, showing modal with props: ", updates)

    showModal(<UpdateAvailablePopup props={updates.newVersion} targetAsset={targetAsset} />, pluginSelf.mainWindow, {
        strTitle: "Millennium Updater",
        popupHeight: 500,
        popupWidth: 725,
    });
}