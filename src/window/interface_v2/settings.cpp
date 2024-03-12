#include "settings.hpp"

std::string ui_interface::settings_page_renderer()
{
    std::string javaScript = R"(
function waitForElement(querySelector, timeout) {
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
}

const init = {
    isActive: false,
    settings: () => {
        const observer = new MutationObserver(() => {
            var setting = document.querySelectorAll('[class*="pagedsettings_PagedSettingsDialog_PageListItem_"]')
            const classList = Array.from(setting[5].classList);

            if (classList.some(className => (/pagedsettings_Active_*/).test(className)) === true) {
                if (!init.isActive) {
                    renderer.modal()
                    init.isActive = true
                }
            }
            else {
                init.isActive = false
            }
        });
        observer.observe(document.body, {
            childList: true,
            subtree: true
        });
    }
}

const IPC = {
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
};

const renderer = {
    modal: async () => {

        var htmlContent = 
        `<div class="S-_LaQG5eEOM2HWZ-geJI gamepaddialog_Field_S-_La qFXi6I-Cs0mJjTjqGXWZA gamepaddialog_WithFirstRow_qFXi6 _3XNvAmJ9bv_xuKx5YUkP-5 gamepaddialog_VerticalAlignCenter_3XNvA _3bMISJvxiSHPx1ol-0Aswn gamepaddialog_WithDescription_3bMIS _3s1Rkl6cFOze_SdV2g-AFo gamepaddialog_WithBottomSeparatorStandard_3s1Rk _1ugIUbowxDg0qM0pJUbBRM gamepaddialog_ChildrenWidthFixed_1ugIU _5UO-_VhgFhDWlkDIOZcn_ gamepaddialog_ExtraPaddingOnChildrenBelow_5UO-_ XRBFu6jAfd5kH9a3V8q_x gamepaddialog_StandardPadding_XRBFu wE4V6Ei2Sy2qWDo_XNcwn gamepaddialog_HighlightOnFocus_wE4V6 Panel" tabindex="-1" style="--indent-level:0">
            <div class="H9WOq6bV_VhQ4QjJS_Bxg gamepaddialog_FieldLabelRow_H9WOq">
                <div class="_3b0U-QDD-uhFpw6xM716fw gamepaddialog_FieldLabel_3b0U-">Steam Skin</div>
                <div class="_2ZQ9wHACVFqZcufK_WRGPM gamepaddialog_FieldChildrenWithIcon_2ZQ9w">
                    <div class="_3N47t_-VlHS8JAEptE5rlR gamepaddialog_FieldChildrenInner_3N47t"><button id="openMillennium" type="type" class="_3epr8QYWw_FqFgMx38YEEm settings_SettingsDialogButton_3epr8 DialogButton _DialogLayout Secondary Focusable" tabindex="-1">Open Millennium</button></div>
                </div>
            </div>
            <div class="_2OJfkxlD3X9p8Ygu1vR7Lr gamepaddialog_FieldDescription_2OJfk">
                <div>Select the skin you wish Steam to use (requires reload) </div>
            </div>
        </div>`

        document.querySelector('.DialogBody.settings_SettingsDialogBodyFade_aFxOa').insertAdjacentHTML('afterbegin', htmlContent);

        waitForElement('#openMillennium').then(({matchedElements}) => {
            console.log("[DEBUG] listening on button")
            matchedElements[0].addEventListener('click', () => {
                window.opener.console.log("millennium.user.message:", JSON.stringify({id: '[open-millennium]'}))
            })
        })
    },
    init: () => {     
        waitForElement('[class*="settings_DesktopPopup_"]').then(({matchedElements}) => {
            init.settings()
        }, 5000).catch(err => {
            window.opener.console.log("millennium.user.message:", JSON.stringify({id: '[open-millennium]'}))
        });
    }
}

//init settings renderer
renderer.init()
)";


    return javaScript;
}