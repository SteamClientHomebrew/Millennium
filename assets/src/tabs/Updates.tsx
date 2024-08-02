import { CSSProperties, useEffect, useState } from 'react'
import {
    Millennium,
    DialogBody,
    DialogBodyText,
    DialogButton,
    DialogControlsSection,
    classMap,
    DialogHeader,
    IconsModule,
    pluginSelf,
    Toggle,
} from '@millennium/ui'
import { Field } from '../custom_components/Field';
import { locale } from '../locales';
import { ThemeItem } from '../types';
import { Settings } from '../Settings';
import { ConnectionFailed } from '../custom_components/ConnectionFailed';
import { SettingsDialogSubHeader } from '../components/SettingsDialogSubHeader';

interface UpdateProps {
    updates: UpdateItemType[];
    setUpdates: React.Dispatch<React.SetStateAction<UpdateItemType[]>>
}

interface UpdateItemType {
    message: string, // Commit message
    date: string, // Humanized since timestamp
    commit: string, // Full GitHub commit URI
    native: string, // Folder name in skins folder. 
    name: string // Common display name
}

const UpToDateModal: React.FC  = () => {

    return (
        <div className="__up-to-date-container" style={{
            display: "flex", flexDirection: "column", alignItems: "center", gap: "20px", height: "100%", justifyContent: "center"
        }}>
            <IconsModule.Checkmark width="64" />
            <div className="__up-to-date-header" style={{marginTop: "-120px", color: "white", fontWeight: "500", fontSize: "15px"}}>{locale.updatePanelNoUpdatesFound}</div>
        </div>
    )
}

const RenderAvailableUpdates: React.FC<UpdateProps> = ({ updates, setUpdates }) => {
    
    const [updating, setUpdating] = useState<Array<any>>([])
    const viewMoreClick = (props: UpdateItemType) => SteamClient.System.OpenInSystemBrowser(props?.commit)
    
    const updateItemMessage = (updateObject: UpdateItemType, index: number) => 
    {
        setUpdating({ ...updating, [index]: true });
        Millennium.callServerMethod("updater.update_theme", {native: updateObject.native})
        .then((result: any) => {
            pluginSelf.connectionFailed = false
            return result
        })
        .then((success: boolean) => 
        {
            /** @todo: prompt user an error occured. */
            if (!success) return 

            const activeTheme: ThemeItem = pluginSelf.activeTheme

            // the current theme was just updated, so reload SteamUI
            if (activeTheme?.native === updateObject?.native) {
                SteamClient.Browser.RestartJSContext()
            }

            Millennium.callServerMethod("updater.get_update_list")
            .then((result: any) => {
                pluginSelf.connectionFailed = false
                return result
            })
            .then((result: any) => {
                setUpdates(JSON.parse(result).updates)
            })
        })
    }

    const fieldButtonsStyles: CSSProperties = {
        display: "flex",
        gap: "8px",
    };
    const updateButtonStyles: CSSProperties = {
        minWidth: "80px",
    };
    const updateDescriptionStyles: CSSProperties = {
        display: "flex",
        flexDirection: "column",
    };
    const updateLabelStyles: CSSProperties = {
        display: "flex",
        alignItems: "center",
        gap: "8px",
    };

    return (
        <DialogControlsSection>
        <SettingsDialogSubHeader>{locale.updatePanelHasUpdates}</SettingsDialogSubHeader>
        <DialogBodyText className='_3fPiC9QRyT5oJ6xePCVYz8'>{locale.updatePanelHasUpdatesSub}</DialogBodyText>

        {updates.map((update: UpdateItemType, index: number) => (
            <>
            <Field
                key={index}
                label=<div style={updateLabelStyles}>
                    <div className="update-item-type" style={{color: "white", fontSize: "12px", padding: "4px", background: "#007eff", borderRadius: "6px"}}>Theme</div>
                    {update.name}
                </div>
                description=<div style={updateDescriptionStyles}>
                    <div><b>{locale.updatePanelReleasedTag}</b> {update?.date}</div>
                    <div><b>{locale.updatePanelReleasePatchNotes}</b> {update?.message}</div>
                </div>
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

const UpdatesViewModal: React.FC = () => {

    const [updates, setUpdates] = useState<Array<UpdateItemType>>(null)
    const [checkingForUpdates, setCheckingForUpdates] = useState<boolean>(false)
    const [showUpdateNotifications, setNotifications] = useState<boolean>(undefined)
    
    useEffect(() => {
        Millennium.callServerMethod("updater.get_update_list")
        .then((result: any) => {
            pluginSelf.connectionFailed = false
            return result
        })
        .then((result: any) => {
            
            const updates = JSON.parse(result)
            console.log(updates)

            setUpdates(updates.updates)
            setNotifications(updates.notifications ?? false)
        })
        .catch((_: any) => {
            console.error("Failed to fetch updates")
            pluginSelf.connectionFailed = true
        })
    }, [])

    const checkForUpdates = async () => {
        if (checkingForUpdates) return 
        
        setCheckingForUpdates(true)
        await Millennium.callServerMethod("updater.re_initialize")

        Millennium.callServerMethod("updater.get_update_list")
        .then((result: any) => {
            pluginSelf.connectionFailed = false
            return result
        })
        .then((result: any) => {
            setUpdates(JSON.parse(result).updates)
            setCheckingForUpdates(false)
        })
        .catch((_: any) => {
            console.error("Failed to fetch updates")
            pluginSelf.connectionFailed = true
        })
    }

    const DialogHeaderStyles: any = {
        display: "flex", alignItems: "center", gap: "15px"
    }

    const OnNotificationsChange = (enabled: boolean) => {

        Millennium.callServerMethod("updater.set_update_notifs_status", { status: enabled})
        .then((result: any) => {
            pluginSelf.connectionFailed = false
            return result
        })
        .then((success: boolean) => {
            if (success) {
                setNotifications(enabled)
                Settings.FetchAllSettings()
            }
        })
    }

    if (pluginSelf.connectionFailed) {
        return <ConnectionFailed/>
    }

    return (
        <>
            <DialogHeader style={DialogHeaderStyles}>
                {locale.settingsPanelUpdates}
                {
                    !checkingForUpdates && 
                    <DialogButton 
                        onClick={checkForUpdates} 
                        className="_3epr8QYWw_FqFgMx38YEEm" 
                        style={{width: "16px", "-webkit-app-region": "no-drag", zIndex: "9999", padding: "4px 4px", display: "flex"} as any}>
                        <IconsModule.Update/>
                    </DialogButton>
                }
            </DialogHeader>
            <DialogBody className={classMap.SettingsDialogBodyFade}>
                <Field
                    label={locale.updatePanelUpdateNotifications}
                    description={locale.updatePanelUpdateNotificationsTooltip}
                >
                    { showUpdateNotifications !== undefined && <Toggle value={showUpdateNotifications} onChange={OnNotificationsChange} /> }
                </Field>
                {updates && (!updates.length ? <UpToDateModal/> : <RenderAvailableUpdates updates={updates} setUpdates={setUpdates}/>)}   
            </DialogBody>
        </>
    )
}

export { UpdatesViewModal }