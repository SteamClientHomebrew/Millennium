/**
 * The following code was injected by Millennium, and is intended for internal use only. 
 * @ src\__builtins__\api.js
 */
const IPC = {
    /** Deprecated */
    send: (messageId, contents) => {
        MILLENNIUM_IPC_SOCKET.send(JSON.stringify({ id: messageId, data: contents }));
    },
    postMessage: (messageId, contents) => {
        return new Promise((resolve, reject) => {
            const message = { id: messageId, iteration: CURRENT_IPC_CALL_COUNT++, data: contents }
            const messageHandler = function(data) {
                const json = JSON.parse(data.data)

                if (json.id != message.iteration)
                    return

                resolve(json);
                MILLENNIUM_IPC_SOCKET.removeEventListener('message', messageHandler);
            };

            MILLENNIUM_IPC_SOCKET.addEventListener('message', messageHandler);
            MILLENNIUM_IPC_SOCKET.send(JSON.stringify(message));
        });
    }
}
window.MILLENNIUM_BACKEND_IPC = IPC

const Millennium = {
    callServerMethod: (pluginName, methodName, kwargs) => {
        return new Promise((resolve, reject) => {
            const query = {
                pluginName: pluginName,
                methodName: methodName
            }
            if (kwargs) query.argumentList = kwargs

            /* call handled from "src\core\ipc\pipe.cpp @ L:67" */
            IPC.postMessage(0, query).then(response => 
            {
                const json = response
                if (json?.failedRequest) {

                    const m = ` Millennium.callServerMethod() from [name: ${pluginName}, method: ${methodName}] failed on exception -> ${json.failMessage}`
                    
                    eval(
`// Millennium can't accurately pin point where this came from
// check the sources tab and find your plugins index.js, and look for a call that could error this
throw new Error(m);
`)
                    reject()
                }

                const val = json.returnValue
                if (typeof val === 'string' || val instanceof String) {
                    resolve(atob(val))
                }
                resolve(val)
            })
        })
    }, 
    AddWindowCreateHook: (callback) => {
        // used to have extended functionality but removed since it was shotty
        g_PopupManager.AddPopupCreatedCallback((e, e1, e2) => {
            callback(e, e1, e2)
        });
    },
    findElement: (privateDocument, querySelector, timeout) => {
        return new Promise((resolve, reject) => {
            const matchedElements = privateDocument.querySelectorAll(querySelector);
            if (matchedElements.length) {
                resolve(matchedElements);
            }
            let timer = null;
            const observer = new MutationObserver(
                () => {
                    const matchedElements = privateDocument.querySelectorAll(querySelector);
                    if (matchedElements.length) {
                        if (timer) clearTimeout(timer);
                        observer.disconnect();
                        resolve(matchedElements);
                    }
                });
            observer.observe(privateDocument.body, {
                childList: true,
                subtree: true
            });
            if (timeout) {
                timer = setTimeout(() => {
                    observer.disconnect();
                    reject();
                }, timeout);
            }
        });
    }
}
window.Millennium = Millennium