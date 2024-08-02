import { Millennium, pluginSelf } from "@millennium/ui"
import { notificationClasses } from "../classes";

/**
 * @todo use builtin notification components instead of altering 
 * SteamClient.ClientNotifications.DisplayClientNotification
 * @param doc document of notification
 */

const RemoveAllListeners = (doc: Document) => {
    var bodyClass = [...doc.getElementsByClassName(notificationClasses.DesktopToastTemplate)];
        
    Array.from(bodyClass).forEach(function(element) {
        var newElement = element.cloneNode(true);
        element.parentNode.replaceChild(newElement, element);
    });
}

const SetClickListener = (doc: Document) => {
    var bodyClass = [...doc.getElementsByClassName(notificationClasses.DesktopToastTemplate)][0];

    bodyClass.addEventListener("click", () => {
        console.log("clicked notif!")
        pluginSelf.OpenOnUpdatesPanel = true

        /** Open the settings window */
        window.open("steam://open/settings", "_blank")
    });
}

const PatchNotification = (doc: Document) => {

    try {
        Millennium.findElement(doc, "." + notificationClasses.GroupMessageTitle).then(async (elements) => {
            const header = (elements[0] as any).innerText
     
            if (header == "Updates Available") {
                (await Millennium.findElement(doc, "." + notificationClasses.StandardLogoDimensions))?.[0]?.remove();
                (await Millennium.findElement(doc, "." + notificationClasses.AvatarStatus))?.[0]?.remove();
                (await Millennium.findElement(doc, "." + notificationClasses.Icon))?.[0]?.remove();
            }
    
            RemoveAllListeners(doc)
            SetClickListener(doc)
        })
    }
    catch (error) {
        console.error(error)
    }
}

export { PatchNotification }