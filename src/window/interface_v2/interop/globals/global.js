import { Millennium } from "../utils/utils.js";
import { combo } from "../utils/combo.js";

window.notifications = [
    { name: "Top Left", data: 0 },
    { name: "Top Right", data: 1 },
    { name: "Bottom Left", data: 2 },
    { name: "Bottom Right", data: 3 }
];

window.corner_prefs = [
    { name: "Default", data: 0 },
    { name: "Force Rounded", data: 1 },
    { name: "Square", data: 2 },
    { name: "Slightly Rounded", data: 3 }
];
//"Default"/*0*/, "Force Rounded"/*0*/, "Square"/*1*/, "Slightly Rounded"/*2*/

window.save_col = () => {
    console.log("millennium.user.message:", JSON.stringify({id: `[set-color-scheme]`, value: document.querySelector("#favcolor").value}))
    window.opener.window.location.reload();
}
  
window.edit_theme = () => {
    Millennium.IPC.postMessage("[edit-theme]").then(code => {
      const statement = atob(code.message)
      window.opener.eval(statement)
    })
}

window.open_theme_store = () => {
    // navigate the store to the millennium theme page
    window.opener.SteamUIStore.Navigate(
      '/browser', 
      //window.opener.MainWindowBrowserManager.LoadURL('http://localhost:3000/themes')
      window.opener.MainWindowBrowserManager.LoadURL('https://millennium.web.app/themes')
    )
}

// open the themes folder
window.open_theme_folder = () => {
    Millennium.IPC.postMessage('[open-theme-folder]', null)
}

window.open_notif_pos = async () => {
    combo.show(
        document.getElementById("notifs-dropdown"), notifications,
        (event) => {
            const pos = parseInt(event.target.getAttribute('data'))

            console.log("millennium.user.message:", JSON.stringify({id: `[set-notifs-pos]`, value: pos}))
            document.getElementById("notifs-pos-current").innerText = notifications.find(item => item.data === pos).name
        }
    )
}

window.border_radius_list = async () => {
    combo.show(
        document.getElementById("corner-pref-dropdown"), corner_prefs,
        (event) => {
            const pos = parseInt(event.target.getAttribute('data'))

            console.log("millennium.user.message:", JSON.stringify({id: `[set-corner-pref]`, value: pos}))
            document.getElementById("corner-pref").innerText = corner_prefs.find(item => item.data === pos).name
        }
    )
}

window.open_theme_list = async () => {

    let query = await Millennium.IPC.getMessage('[get-theme-list]')
    let message = JSON.parse(query.message)

    let themes = [
        { name: "< default skin >", data: "default" }
    ]

    message.forEach(({ name, "native-name": native }) => {
        themes.push({
        name: name,
        data: native
        });
    });

    combo.show(
        document.getElementById("theme-dropdown"), themes,
        (event) => {
            const pos = event.target.getAttribute('data')

            console.log('changing theme', pos)
            Millennium.IPC.postMessage("[update-theme-select]", pos);
        }
    )
}