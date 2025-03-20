import { CSSProperties, useEffect, useState } from 'react'
import { DialogBodyText, DialogButton, DialogControlsSection, IconsModule, pluginSelf, Toggle, SteamSpinner, Field, callable, ConfirmModal, showModal, } from '@steambrew/client'
import { locale } from '../locales';
import { ThemeItem, UpdaterOptionProps } from '../types';
import { Settings } from '../Settings';
import { ConnectionFailed } from '../custom_components/ConnectionFailed';
import { SettingsDialogSubHeader } from '../components/ISteamComponents';

interface UpdateProps {
    updates: UpdateItemType[]
    fetchUpdates: () => Promise<boolean>
}

interface UpdateItemType {
    message: string, // Commit message
    date: string, // Humanized since timestamp
    commit: string, // Full GitHub commit URI
    native: string, // Folder name in skins folder. 
    name: string // Common display name
}

const UpToDateModal: React.FC = () => {

    return (
        <div className="__up-to-date-container" style={{
            display: "flex", flexDirection: "column", alignItems: "center", gap: "20px", height: "100%", justifyContent: "center"
        }}>
            <IconsModule.Checkmark width="64" />
            <div className="__up-to-date-header" style={{ marginTop: "-120px", color: "white", fontWeight: "500", fontSize: "15px" }}>{locale.updatePanelNoUpdatesFound}</div>
        </div>
    )
}

const UpdateTheme = callable<[{ native: string }], boolean>("updater.update_theme")

const ShowMessageBox = (message: string, title?: string) => {
    return new Promise((resolve) => {
        const onOK = () => resolve(true)
        const onCancel = () => resolve(false)

        showModal(<ConfirmModal
            // @ts-ignore
            strTitle={title ?? LocalizationManager.LocalizeString("#InfoSettings_Title")}
            strDescription={message}
            onOK={onOK}
            onCancel={onCancel}
        />,
            pluginSelf.windows["Millennium"],
            {
                bNeverPopOut: false,
            },
        );
    })
}

interface FormatString {
    (template: string, ...args: string[]): string;
}

const formatString: FormatString = (template, ...args) => {
    return template.replace(/{(\d+)}/g, (match, index) => {
        return index < args.length ? args[index] : match;  // Replace {index} with the corresponding argument or leave it unchanged
    });
}

const RenderAvailableUpdates: React.FC<UpdateProps> = ({ updates, fetchUpdates }) => {

    const [updating, setUpdating] = useState<Array<any>>([])
    const viewMoreClick = (props: UpdateItemType) => SteamClient.System.OpenInSystemBrowser(props?.commit)

    const updateItemMessage = async (updateObject: UpdateItemType, index: number) => {
        setUpdating({ ...updating, [index]: true });

        const success = await UpdateTheme({ native: updateObject.native })

        if (success) {
            /** Check for updates */
            await fetchUpdates()
            const activeTheme: ThemeItem = pluginSelf.activeTheme

            /** Check if the updated theme is currently in use, if so reload */
            if (activeTheme?.native === updateObject?.native) {
                SteamClient.Browser.RestartJSContext()

                // @ts-ignore
                const reload = await ShowMessageBox(formatString(locale.updateSuccessfulRestart, updateObject?.name), LocalizationManager.LocalizeString("#ImageUpload_SuccessCard"))

                if (reload) {
                    SteamClient.Browser.RestartJSContext()
                }
            }
            else {
                // @ts-ignore=
                const reload = await ShowMessageBox(formatString(locale.updateSuccessful, updateObject?.name), LocalizationManager.LocalizeString("#ImageUpload_SuccessCard"))

                if (reload) {
                    SteamClient.Browser.RestartJSContext()
                }
            }
        }
        else {
            // @ts-ignore
            ShowMessageBox(formatString(locale.updateFailed, updateObject?.name), LocalizationManager.LocalizeString("#Generic_Error"))
        }
    }
    const fieldButtonsStyles: CSSProperties = { display: "flex", gap: "8px", };
    const updateButtonStyles: CSSProperties = { minWidth: "80px" };
    const updateDescriptionStyles: CSSProperties = { display: "flex", flexDirection: "column" };
    const updateLabelStyles: CSSProperties = { display: "flex", alignItems: "center", gap: "8px" };

    return (
        <DialogControlsSection>
            <SettingsDialogSubHeader>{locale.updatePanelHasUpdates}</SettingsDialogSubHeader>
            <DialogBodyText className='_3fPiC9QRyT5oJ6xePCVYz8'>{locale.updatePanelHasUpdatesSub}</DialogBodyText>

            {updates.map((update: UpdateItemType, index: number) => (
                <>
                    <Field
                        key={index}
                        label={<div style={updateLabelStyles}>
                            <div className="update-item-type" style={{ color: "white", fontSize: "12px", padding: "4px", background: "#007eff", borderRadius: "6px" }}>Theme</div>
                            {update.name}
                        </div>}
                        description={<div style={updateDescriptionStyles}>
                            <div><b>{locale.updatePanelReleasedTag}</b> {update?.date}</div>
                            <div><b>{locale.updatePanelReleasePatchNotes}</b> {update?.message}</div>
                        </div>}
                        bottomSeparator={updates.length - 1 === index ? 'none' : 'standard'}
                    >
                        <div style={fieldButtonsStyles}>
                            <DialogButton
                                onClick={() => viewMoreClick(update)}
                                style={updateButtonStyles}
                                className="_3epr8QYWw_FqFgMx38YEEm">
                                {locale.ViewMore}
                            </DialogButton>
                            <DialogButton
                                onClick={() => updateItemMessage(update, index)}
                                style={updateButtonStyles}
                                className="_3epr8QYWw_FqFgMx38YEEm">
                                {updating[index] ? locale.updatePanelIsUpdating : locale.updatePanelUpdate}
                            </DialogButton>
                        </div>
                    </Field>
                </>
            ))}
        </DialogControlsSection>
    )
}

const GetUpdateList = callable<[{ force: boolean }], any>("updater.get_update_list")

const UpdatesViewModal: React.FC = () => {

    const [updates, setUpdates] = useState<Array<UpdateItemType>>(null)
    const [hasReceivedUpdates, setHasReceivedUpdates] = useState<boolean>(false)

    const FetchAvailableUpdates = async (): Promise<boolean> => new Promise(async (resolve, reject) => {
        try {
            const updateList = JSON.parse(await GetUpdateList({ force: false }))
            pluginSelf.connectionFailed = false

            setUpdates(updateList.updates)
            setHasReceivedUpdates(true)
            resolve(true)
        }
        catch (exception) {
            console.error("Failed to fetch updates")
            pluginSelf.connectionFailed = true
            reject(false)
        }
        resolve(true)
    })

    useEffect(() => { FetchAvailableUpdates() }, [])

    /** Check if the connection failed, this usually means the backend crashed or couldn't load */
    if (pluginSelf.connectionFailed) {
        return <ConnectionFailed />
    }

    return !hasReceivedUpdates ?
        <SteamSpinner background={"transparent"} /> : <>
            {updates && (!updates.length ? <UpToDateModal /> : <RenderAvailableUpdates updates={updates} fetchUpdates={FetchAvailableUpdates} />)}
        </>

}

export { UpdatesViewModal }