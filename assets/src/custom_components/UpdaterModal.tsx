import Markdown from 'markdown-to-jsx'
import { callable, ConfirmModal, DialogBody, DialogBodyText, DialogButton, DialogFooter, DialogHeader, Field, ModalPosition, pluginSelf, showModal, ShowModalResult, Toggle } from "@steambrew/client";
import { Separator } from "./PageList";
import { useState } from "react";
import { UpdaterOptionProps } from '../types';
import { locale } from '../locales';
import { GenericConfirmDialog } from './GenericDialog';
import { ShowDownloadInformation } from './modals/UpdateInfoModal';
import { PromptSelectUpdaterOptions } from './modals/SelectUpdatePrefs';
import { OnDoNotShowAgainChange } from './modals/DisableUpdates';

const UpdateMillennium = callable<[{ downloadUrl: string }], void>("MillenniumUpdater.queue_update");

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

const MakeAnchorExternalLink = ({ children, ...props }: { children: any, props: any }) => (
    <a target="_blank" {...props}>{children}</a>
)

const UpdateAvailablePopup = ({ props, targetAsset }: { props: any, targetAsset: any }) => {
    const [doNotShowAgain, setDoNotShowAgain] = useState(false);

    const OpenDiffInExternalBrowser = () => {
        SteamClient.System.OpenInSystemBrowser(`https://github.com/shdwmtr/millennium/compare/${pluginSelf.version}...${props.tag_name}`)
    }

    return (
        <GenericConfirmDialog>
            <DialogHeader> Update Available! 💫 </DialogHeader>
            <DialogBody>
                <DialogBodyText style={{ overflowY: "scroll", marginBottom: "unset" }}>
                    <p className="updateInfoTidbit">{locale.strAnUpdateIsAvailable}</p>
                    <Separator />
                    <Markdown options={{ overrides: { a: { component: MakeAnchorExternalLink } } }} className="MillenniumReleaseMarkdown">
                        {props.body}
                    </Markdown>
                </DialogBodyText>

                <DialogFooter style={{ flexDirection: "column" }}>
                    <div className="UpdateLinksContainer" style={{ display: "flex", gap: "10px", justifyContent: "flex-end" }}>
                        <a className="viewDiffLink" onClick={OpenDiffInExternalBrowser} style={ContainerURLStyles} >
                            {locale.strViewUpdateDiffInBrowser}
                        </a>
                        <a className="viewDownloadInfo" onClick={ShowDownloadInformation.bind(null, props, targetAsset)} style={ContainerURLStyles} >
                            {locale.strViewDownloadInfo}
                        </a>
                    </div>
                    <div className="DialogTwoColLayout _DialogColLayout Panel">
                        <Field label={locale.strDontShowAgain} bottomSeparator="none">
                            <Toggle value={doNotShowAgain} onChange={(newValue) => OnDoNotShowAgainChange(newValue, setDoNotShowAgain)} />
                        </Field>

                        <DialogButton className='Primary' onClick={UpdateMillennium.bind(null, { downloadUrl: targetAsset.browser_download_url })}>
                            {locale.strUpdateNextStartup}
                        </DialogButton>
                        <DialogButton className='Secondary' onClick={() => { pluginSelf?.millenniumUpdaterWindow?.SteamClient?.Window?.Close() }}>
                            {locale.strUpdateReject}
                        </DialogButton>
                    </div>
                </DialogFooter>
            </DialogBody>
        </GenericConfirmDialog>
    )
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
    if (updates.hasUpdate === false) {
        LOG_UPDATE_INFO("No updates available, skipping...")
        return;
    }

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
        UpdateMillennium({ downloadUrl: targetAsset.browser_download_url })
        return;
    }

    LOG_UPDATE_INFO("Updates available, showing modal with props: ", updates)

    showModal(<UpdateAvailablePopup props={updates.newVersion} targetAsset={targetAsset} />, pluginSelf.mainWindow, {
        strTitle: "Millennium Updater",
        popupHeight: 500,
        popupWidth: 725,
    });
}