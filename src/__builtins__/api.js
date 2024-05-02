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
    },
    createWindow: (controls) => {

        const WindowControls = {
          Minimize: 1 << 0,
          Maximize: 1 << 1,
          Close: 1 << 2
        };

        function CreateWindow(windowOptions) {
          let wnd = window.open(
            `about:blank?createflags=274&minwidth=${controls?.minimumDimensions?.width ?? 0}&minheight=${controls?.minimumDimensions?.height ?? 0}`,
            undefined, Object.entries(windowOptions).map(e => e.join('=')).join(','), false
          );
      
          if ((!controls?.emptyDocument) ?? true) {
            wnd.document.write(`
              <html class="client_chat_frame fullheight ModalDialogPopup MillenniumWindow" lang="en">
                <head>
                  <link href="/css/library.css" rel="stylesheet">
                  <link rel="stylesheet" type="text/css" href="/css/libraries/libraries~2dcc5aaf7.css">
                  <link rel="stylesheet" type="text/css" href="/css/chunk~2dcc5aaf7.css">
                  <title>${controls?.title ?? "Steam"}</title>
                </head>
                <body class="fullheight ModalDialogBody DesktopUI">
                  <div id="popup_target" class="fullheight">
                    <div class="PopupFullWindow">
                      <div class="TitleBar title-area">
                        <div class="title-area-highlight"></div>
                        <div class="title-area-children"></div>
                        <div class="title-bar-actions window-controls">
                          ${
                            controls?.controls & WindowControls.Close ?
                            `<div class="title-area-icon closeButton windowControlButton">
                            <div class="title-area-icon-inner">
                              <svg version="1.1" id="Layer_2" xmlns="http://www.w3.org/2000/svg" class="SVGIcon_Button SVGIcon_X_Line" x="0px" y="0px" width="256px" height="256px" viewBox="0 0 256 256">
                                <line fill="none" stroke="#FFFFFF" stroke-width="45" stroke-miterlimit="10" x1="212" y1="212" x2="44" y2="44"></line>
                                <line fill="none" stroke="#FFFFFF" stroke-width="45" stroke-miterlimit="10" x1="44" y1="212" x2="212" y2="44"></line>
                              </svg>
                            </div>
                          </div>`: ''
                          }
                          ${
                            controls?.controls & WindowControls.Maximize ?
                            `<div class="title-area-icon maximizeButton windowControlButton">
                            <div class="title-area-icon-inner">
                              <svg version="1.1" id="base" xmlns="http://www.w3.org/2000/svg" class="SVGIcon_Button SVGIcon_Maximize" x="0px" y="0px" width="256px" height="256px" viewBox="0 0 256 256">
                                <rect x="24" y="42.167" fill="none" stroke='rgb(120, 138, 146)' stroke-width="18" stroke-miterlimit="10" width="208" height="171.667"></rect>
                                <line fill="none" stroke='rgb(120, 138, 146)' stroke-width="42" stroke-miterlimit="10" x1="24" y1="54.01" x2="232" y2="54.01"></line>
                              </svg>
                            </div>
                          </div>`: ''
                          }
                          ${
                              controls?.controls & WindowControls.Minimize ?
                              `<div class="title-area-icon minimizeButton windowControlButton">
                              <div class="title-area-icon-inner">
                                <svg version="1.1" id="base" xmlns="http://www.w3.org/2000/svg" class="SVGIcon_Button SVGIcon_Minimize" x="0px" y="0px" width="256px" height="256px" viewBox="0 0 256 256">
                                  <line fill="none" stroke='rgb(120, 138, 146)' stroke-width="18" stroke-miterlimit="10" x1="24" y1="209.01" x2="232" y2="209.01"></line>
                                </svg>
                              </div>
                            </div>` : ''
                          }
                        </div>
                      </div>
                      <div class="FullModalOverlay" style="display: none;">
                        <div class="ModalOverlayContent ModalOverlayBackground"></div>
                      </div>
                      <div class="ModalPosition" tabindex="0">
                        <div class="ModalPosition_Content">
                          <div class="ModalPosition_TopBar"></div>
                          <div class="ModalPosition_Dismiss">
                            <div class="closeButton">
                              <svg version="1.1" id="Layer_2" xmlns="http://www.w3.org/2000/svg" class="SVGIcon_Button SVGIcon_X_Line" x="0px" y="0px" width="256px" height="256px" viewBox="0 0 256 256">
                                <line fill="none" stroke="#FFFFFF" stroke-width="45" stroke-miterlimit="10" x1="212" y1="212" x2="44" y2="44"></line>
                                <line fill="none" stroke="#FFFFFF" stroke-width="45" stroke-miterlimit="10" x1="44" y1="212" x2="212" y2="44"></line>
                              </svg>
                            </div>
                          </div>
                          <div class="_3I6h_oySuLmmLY9TjIKT9s _3few7361SOf4k_YuKCmM62 MzjwfxXSiSauz8kyEuhYO Panel" tabindex="-1">
                            <div class="DialogContentTransition Panel" tabindex="-1" style="width: 100vw !important; min-width: 100vw !important;">
                              <div class="DialogContent _DialogLayout _1I3NifxqTHCkE-2DeritAs " style="padding: 24px !important;">
                                <div class="DialogContent_InnerWidth">
                                  <div class="DialogHeader">${controls?.title ?? "Steam"}</div>
                                    <div class="inner_scroll_region" id="millenniumWindowContentRegion">
                                      
                                    </div>
                                  </div>
                                </div>
                              </div>
                            </div>
                          </div>
                        </div>
                      </div>
                    </div>
                    <div class="window_resize_grip"></div>
                  </div>
                </body>
              
                <script>
                  ${
                    controls?.controls & WindowControls.Maximize ?
                    `document.querySelector(".maximizeButton").addEventListener('click', () => {
                      SteamClient.Window.ToggleMaximize()
                    })` : ''
                  }
                  ${
                    controls?.controls & WindowControls.Close ? 
                    `document.querySelector(".closeButton").addEventListener('click', () => {
                      SteamClient.Window.Close()
                  })`: ''
                  }
                  ${controls?.controls & WindowControls.Minimize ? `         
                  document.querySelector(".minimizeButton").addEventListener('click', () => {
                    SteamClient.Window.Minimize()
                  })` : ""}

                </script>
              
              <style>
              .DialogContent_InnerWidth {
                  margin-top: 0px !important;
                  overflow: hidden !important;
              }
              .inner_scroll_region {
                  overflow: scroll;
                  margin-bottom: 100px;
              }
              </style>
                  
              </html>`
            )
          }

          setTimeout(() => {
            if (controls?.resizable ?? true) {
              wnd.SteamClient.Window.SetResizeGrip(8, 8)
            }
            if (controls?.autoShow ?? true) {
                wnd.SteamClient.Window.ShowWindow()
            }
          }, (controls?.delayShow ?? 500));
          return wnd
        }

        let width = controls?.dimensions?.width ?? 850
        let height = controls?.dimensions?.height ?? 600

        SteamClient.Window.GetDefaultMonitorDimensions().then(res => {
          
            const vw = res.nUsableWidth;
            const vh = res.nUsableHeight;

            let wnd = CreateWindow({
                width: width,
                height: height,
                left: (vw / 2) - (width / 2),
                top: (vh / 2) - (height / 2),
                createflags: 274
            });
            return {window: wnd, document: wnd.document}
        })
    }
}
window.Millennium = Millennium