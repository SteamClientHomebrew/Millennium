import { callable, DialogBody, DialogBodyText, DialogButton, DialogFooter, DialogHeader, Field, pluginSelf, showModal, ShowModalResult, Toggle } from "@steambrew/client";
import { GenericConfirmDialog } from "../GenericDialog";
import { useState } from "react";
import { UpdaterOptionProps } from "../../types";
import { locale } from "../../locales";
import { Separator } from "../../components/ISteamComponents";

const SetUserWantsUpdates = callable<[{ wantsUpdates: boolean }], void>("MillenniumUpdater.set_user_wants_updates");
const SetUserWantsNotifications = callable<[{ wantsNotify: boolean }], void>("MillenniumUpdater.set_user_wants_update_notify");

export const PromptSelectUpdaterOptions = () => {
    /** Handle to the security window */
    let SecurityModalWindow: ShowModalResult;

    const RenderOptionSelector = () => {
        const [wantsUpdates, setWantsUpdates] = useState(
            pluginSelf.wantsMillenniumUpdates == UpdaterOptionProps.UNSET || pluginSelf.wantsMillenniumUpdates == UpdaterOptionProps.YES
        );
        const [wantsNotify, setWantsNotify] = useState(
            pluginSelf.wantsMillenniumUpdateNotifications == UpdaterOptionProps.UNSET || pluginSelf.wantsMillenniumUpdateNotifications == UpdaterOptionProps.YES
        );

        const OnUpdateChange = (newValue: boolean) => {
            setWantsUpdates(newValue)
        }

        const OnNotifyChange = (newValue: boolean) => {
            setWantsNotify(newValue)
        }

        const OnContinue = () => {
            SetUserWantsUpdates({ wantsUpdates: wantsUpdates });
            SetUserWantsNotifications({ wantsNotify: wantsNotify });
            SecurityModalWindow?.Close()
        }

        return (
            <GenericConfirmDialog>
                <DialogHeader>Millennium</DialogHeader>
                <DialogBody>
                    <DialogBodyText>
                        <p className="updateInfoTidbit">
                            {locale.message1162025SecurityUpdate}
                            <br /><br />
                            <b>{locale.message1162025SecurityUpdateTooltip}</b>
                        </p>

                        <Separator />

                        <Field label={locale.toggleWantsMillenniumUpdates} description={locale.toggleWantsMillenniumUpdatesTooltip} bottomSeparator="none">
                            <Toggle value={wantsUpdates} onChange={OnUpdateChange} />
                        </Field>
                        <Field label={locale.toggleWantsMillenniumUpdatesNotifications} description={locale.toggleWantsMillenniumUpdatesNotificationsTooltip} bottomSeparator="none">
                            <Toggle value={wantsNotify} onChange={OnNotifyChange} />
                        </Field>
                    </DialogBodyText>

                    <DialogFooter>
                        <div className="DialogTwoColLayout _DialogColLayout Panel" style={{ flexDirection: "row", justifyContent: "space-between" }}>
                            <p className='updateInfoTidbit' style={{ lineHeight: "7px", fontSize: "14px" }}>{locale.settingsAreChangeableLater}</p>
                            <DialogButton className='Primary' onClick={OnContinue}>
                                Continue
                            </DialogButton>
                        </div>
                    </DialogFooter>
                </DialogBody>
            </GenericConfirmDialog>
        )
    }

    SecurityModalWindow = showModal(<RenderOptionSelector />, pluginSelf.mainWindow, {
        strTitle: "Select Update Options",
        popupHeight: 545,
        popupWidth: 675,
    })
}