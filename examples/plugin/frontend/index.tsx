import { Millennium } from "millennium-lib"; 

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

// Entry point on the front end of your plugin
export default async function PluginMain() {

    // const startTime = Date.now();

    // const result = await Millennium.callServerMethod("Backend.receive_frontend_message", {
    //     message: "Hello World From Frontend!",
    //     status: true,
    //     count: 69
    // })
    // console.log(`callServerMethod took: ${Date.now() - startTime} ms`);

    // const steam_path = await Millennium.callServerMethod("get_steam_path")

    // console.log("Result from callServerMethod:", result)
    // console.log("Steam Path:", steam_path)
    Millennium.AddWindowCreateHook(windowCreated)
}
