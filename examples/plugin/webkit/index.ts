/**
 * You have a limited version of the Millennium API available to you in the webkit context.
 */
type Millennium = {
    /**
     * Call a function in the backend (python) of your plugin
     * @param methodName a public static method declared and defined in your plugin backend (python)
     * @param kwargs an object of key value pairs that will be passed to the backend method. accepted types are: string, number, boolean
     * @returns string | number | boolean
     */
    callServerMethod: (methodName: string, kwargs?: any) => Promise<any>,
    /**
     * Async wait for an element in the document using DOM observers and querySelector
     * @param privateDocument document object of the webkit context
     * @param querySelector the querySelector string to find the element (as if you were using document.querySelector)
     * @param timeOut If the element is not found within the timeOut, the promise will reject
     * @returns 
     */
    findElement: (privateDocument: Document,  querySelector: string, timeOut?: number) => Promise<NodeListOf<Element>>,
};

declare const Millennium: Millennium;

export default async function WebkitMain () {
    const message = await Millennium.callServerMethod("Backend.receive_frontend_message", {
        message: "Hello World From Frontend!",
        status: true,
        count: 69
    })

    console.log("Result from callServerMethod:", message)
}