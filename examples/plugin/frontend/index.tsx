import { callable, Millennium } from '@steambrew/client';
import { ExampleSettingsWithTabs } from './ExampleSettingsWithTabs';

class classname {
    static method(country: string, age: number) {
        console.log(`age: ${age}, country: ${country}`);
        return "method called"
    }
}
// export classname class to global context
Millennium.exposeObj({ classname });

function windowCreated(context: object) 
{
    // window create event. 
    // you can interact directly with the document and monitor it with dom observers
    // you can then render components in specific pages. 
    console.log(context)
}

// Declare a function that exists on the backend
const backendMethod = callable<[{ message: string, status: boolean, count: number }], boolean>('Backend.receive_frontend_message')

const settings = Millennium.exposeSettings(new ExampleSettingsWithTabs());

// Entry point on the front end of your plugin
export default async function PluginMain() {
    // Call the backend method
    const message = await backendMethod({ message: settings.BasicSettings.frontendMessage, status: true, count: settings.BasicSettings.frontendCount })
    console.log("Result from callServerMethod:", message)

    Millennium.AddWindowCreateHook(windowCreated)
}
