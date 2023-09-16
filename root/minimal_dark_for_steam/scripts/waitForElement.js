/*jshint esversion: 6 */
/*code from Laser*/
/**
 * Wait for an element before resolving a promise
 * @param {String} querySelector - Selector of element to wait for
 * @param {Integer} timeout - Milliseconds to wait before timing out, or 0 for no timeout
 * @returns {Promise<{querySelector: string; matchedElements: Element[] }>}  resolves with original selector & found elements
 */
export function waitForElement(querySelector, timeout) {
    return new Promise((resolve, reject) => {
        /* Do a quick check so see if the element is already in the dom */
        const matchedElements = document.querySelectorAll(querySelector);
        if (matchedElements.length) {
            resolve({
                querySelector,
                matchedElements
            });
        }

        let timer = null;

        /* Element wasn't already in the dom, so we shall listen to changes for a while */
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

        // watch changes to the body & its sub elements
        observer.observe(document.body, {
            childList: true,
            subtree: true
        });

        /* Enforce the timeout */
        if (timeout) {
            timer = setTimeout(() => {
                observer.disconnect(); // clean up
                reject(querySelector); // we didn't find anything in time :(
            }, timeout);
        }
    });
}