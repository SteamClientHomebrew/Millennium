import { callable, Millennium, staticEmbed, PopupManager, PopupProps } from "@steambrew/client";

class classname {
    static method(country: string, age: number) {
        console.log(`age: ${age}, country: ${country}`);
        return "method called"
    }
}

const cssAsset = staticEmbed("../styles/styles.css")

// export classname class to global context
Millennium.exposeObj({ classname });

function windowCreated(context: PopupProps) {
    // window create event. 
    // you can interact directly with the document and monitor it with dom observers
    // you can then render components in specific pages. 
    console.log(context)

    // here we are using `g_PopupManager`, which is Steams window manager. 
    // every time a new window is created, we check all open windows for the context menu, and then we add our css to it.
    PopupManager.m_mapPopups.data_.forEach(popup => {
        if (popup.value_._strName === "contextmenu_1") {
            popup.value_.m_popup.document.head.appendChild(cssAsset.toDOMElement())
        }
    })
}

// Declare a function that exists on the backend
const backendMethod = callable<[{ message: string, status: boolean, count: number }], boolean>('Backend.receive_frontend_message')

// Entry point on the front end of your plugin
export default async function PluginMain() {

    console.log(cssAsset.contents)

    // Call the backend method
    const message = await backendMethod({ message: "Hello World From Frontend!", status: true, count: 69 })
    console.log("Result from callServerMethod:", message)

    Millennium.AddWindowCreateHook(windowCreated)
}
