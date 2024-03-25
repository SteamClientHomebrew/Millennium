const Millennium = {
    waitFor: (querySelector, timeout) => {
        return new Promise((resolve, reject) => {
            const matchedElements = document.querySelectorAll(querySelector);
            if (matchedElements.length) {
                resolve({
                    querySelector,
                    matchedElements
                });
            }
            let timer = null;
            const observer = new MutationObserver(
                () => {
                    const matchedElements = document.querySelectorAll(querySelector);
                    if (matchedElements.length) {
                        if (timer) clearTimeout(timer);
                        observer.disconnect();
                        resolve({
                            querySelector,
                            matchedElements
                        });
                    }
                });
            observer.observe(document.body, {
                childList: true,
                subtree: true
            });
            if (timeout) {
                timer = setTimeout(() => {
                    observer.disconnect();
                    reject(querySelector);
                }, timeout);
            }
        });
    },
    
    IPC: {
        getMessage: (messageId) => {
            window.opener.console.log("millennium.user.message:", JSON.stringify({id: messageId}))
    
            return new Promise((resolve) => {
                var originalConsoleLog = window.opener.console.log;
    
                window.opener.console.log = function(message) {
                    originalConsoleLog.apply(console, arguments);
    
                    if (message.returnId === messageId) {
                        window.opener.console.log = originalConsoleLog;
                        resolve(message);
                    }
                };
            });
        },
        postMessage: (messageId, contents) => {
            window.opener.console.log("millennium.user.message:", JSON.stringify({id: messageId, data: contents}))
    
            return new Promise((resolve) => {
                var originalConsoleLog = window.opener.console.log;
    
                window.opener.console.log = function(message) {
                    originalConsoleLog.apply(console, arguments);
    
                    if (message.returnId === messageId) {
                        window.opener.console.log = originalConsoleLog;
                        resolve(message);
                    }
                };
            });
        }
    }
}

export { Millennium }