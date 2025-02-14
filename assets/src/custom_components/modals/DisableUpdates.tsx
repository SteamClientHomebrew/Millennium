import { callable, ConfirmModal, pluginSelf, showModal } from "@steambrew/client";
import { locale } from "../../locales";

const SetUserWantsUpdates = callable<[{ wantsUpdates: boolean }], void>("MillenniumUpdater.set_user_wants_updates");
const SetUserWantsNotifications = callable<[{ wantsNotify: boolean }], void>("MillenniumUpdater.set_user_wants_update_notify");

const ReEnableUpdates = (setDoNotShowAgain: React.Dispatch<React.SetStateAction<boolean>>) => {
    const OnContinue = () => {
        SetUserWantsUpdates({ wantsUpdates: true });
        SetUserWantsNotifications({ wantsNotify: true });
        setDoNotShowAgain(false)
    }

    showModal(<ConfirmModal
        strTitle="Re-enable Updates?"
        strDescription="Do you want to re-enable update checks? You can always change this in Millennium Settings later."
        bHideCloseIcon={true}
        strOKButtonText="Yes"
        strCancelButtonText="No"
        onOK={OnContinue}

    />, pluginSelf?.windows?.["Millennium Updater"], {});
}

const DisableUpdates = (setDoNotShowAgain: React.Dispatch<React.SetStateAction<boolean>>) => {
    /** Disable update checking, and the ability to update as a whole. */
    const DisableUpdates = () => {
        SetUserWantsUpdates({ wantsUpdates: false });
        setDoNotShowAgain(true)
    }

    /** Disable update notifications, in this scenario, if update checking is on, updates will be automatically received. */
    const DisableNotifications = () => {
        SetUserWantsNotifications({ wantsNotify: false });
        setDoNotShowAgain(true)
    }

    showModal(
        <ConfirmModal
            strTitle={locale.messageTitleWarning}
            strDescription={locale.messageUpdateDisableClarification}
            strOKButtonText={locale.DisableUpdates}
            strMiddleButtonText={locale.DisableOnlyNotifications}
            onOK={DisableUpdates}
            onMiddleButton={DisableNotifications}
            bHideCloseIcon={true}
        />,
        pluginSelf?.windows?.["Millennium Updater"],
        {}
    );
}

export const OnDoNotShowAgainChange = (wantsDisableUpdates: boolean, setDoNotShowAgain: React.Dispatch<React.SetStateAction<boolean>>) => {
    switch (wantsDisableUpdates) {
        case true: DisableUpdates(setDoNotShowAgain); break;
        case false: ReEnableUpdates(setDoNotShowAgain); break;
    }
}