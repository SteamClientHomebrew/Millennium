const socket = new WebSocket('ws://localhost:3242');

var settings_html = `
<div class="DialogSubHeader settings_SettingsDialogSubHeader_2rK4Y">Millennium Settings</div>
<div tabindex="-1" style="--indent-level:0;" class="gamepaddialog_Field_S-_La gamepaddialog_WithFirstRow_qFXi6 gamepaddialog_VerticalAlignCenter_3XNvA gamepaddialog_WithDescription_3bMIS gamepaddialog_WithBottomSeparatorStandard_3s1Rk gamepaddialog_ChildrenWidthFixed_1ugIU gamepaddialog_ExtraPaddingOnChildrenBelow_5UO-_ gamepaddialog_StandardPadding_XRBFu gamepaddialog_HighlightOnFocus_wE4V6 Panel Focusable">
   <div>
      <div class="gamepaddialog_FieldLabelRow_H9WOq">
         <div class="gamepaddialog_FieldLabel_3b0U-">Steam Skin</div>
         <div class="gamepaddialog_FieldChildren_14_HB">
            <div id="openSkinPath" title="open skins folder" style="margin-right: -3px;" class="DialogDropDown _DialogInputContainer Panel Focusable Active" tabindex="-1">
               ...
            </div>
         </div>
         <div class="gamepaddialog_FieldChildren_14_HB">
            <select id="skinsSelect" class="DialogDropDown _DialogInputContainer Panel Focusable Active" tabindex="-1">
            </select>
         </div>
      </div>
      <div style="display: flex; justify-content: space-between;">
         <div class="gamepaddialog_FieldDescription_2OJfk" style="margin-top: 0px;">Select a skin for Millennium to use!</div>
         <div style="font-size: 10px; margin-top: 0px; text-align: right;"class="gamepaddialog_FieldDescription_2OJfk">millennium+prod.client-v1.0.1</div>
      </div>
   </div>
</div>
<div class="gamepaddialog_Field_S-_La gamepaddialog_WithFirstRow_qFXi6 gamepaddialog_VerticalAlignCenter_3XNvA gamepaddialog_WithDescription_3bMIS gamepaddialog_WithBottomSeparatorStandard_3s1Rk gamepaddialog_ExtraPaddingOnChildrenBelow_5UO-_ gamepaddialog_StandardPadding_XRBFu gamepaddialog_HighlightOnFocus_wE4V6 Panel Focusable" tabindex="-1" style="--indent-level:0;">
   <div class="gamepaddialog_FieldLabelRow_H9WOq">
      <div class="gamepaddialog_FieldLabel_3b0U-">Allow Javascript Injection</div>
      <div class="gamepaddialog_FieldChildren_14_HB">
         <div class="gamepaddialog_Toggle_24G4g gamepaddialog_On_3ld7T Focusable" id="enable_js" tabindex="-1">
            <div class="gamepaddialog_ToggleRail_2JtC3"></div>
            <div class="gamepaddialog_ToggleSwitch_3__OD"></div>
         </div>
      </div>
   </div>
   <div class="gamepaddialog_FieldDescription_2OJfk">This setting allows skins to use javascript, which can potentially be malicious. disabling javascript may break the skin you use so use skins of developers you trust.<br><b>USE THIS SETTING AT YOUR OWN RISK</b></div>
   <div class="DialogControlsSection librarysettings_ActionSection_RoSh6">
      <button type="button" class="settings_SettingsDialogButton_3epr8 DialogButton _DialogLayout Secondary Focusable" tabindex="-1" id="github_link">GitHub</button>
      <button type="button" class="settings_SettingsDialogButton_3epr8 DialogButton _DialogLayout Secondary Focusable" tabindex="-1" id="discordServer">Discord</button>
      <button type="button" class="settings_SettingsDialogButton_3epr8 DialogButton _DialogLayout Secondary Focusable" tabindex="-1" id="donate_link">Donate</button>
   </div>
</div>
<div class="DialogSubHeader settings_SettingsDialogSubHeader_2rK4Y">Steam Settings</div>
`;

let ipc_types = {
    open_skins_folder: 0, skin_update: 1, open_url: 2, change_javascript: 3, change_console: 4, get_skins: 5,   
}

function millennium_ipc(value) { socket.send(JSON.stringify(value)) }

function handle_interface()
{
    const checked = "gamepaddialog_On_3ld7T"

    const select = document.getElementById("skinsSelect");
    const enable_javascript = document.querySelector("#enable_js");

    millennium_ipc({type: ipc_types.get_skins})

    select.addEventListener("change", (event) => {
        const value = event.target.value;
        console.log("changing skin")
        millennium_ipc({type: 1, content: value})
    });

    select.addEventListener("click", (event) => {
        console.log('clicked skins select')
        millennium_ipc({type: ipc_types.get_skins})
    });

    socket.onmessage = function(event) 
    {
        const msg = JSON.parse(event.data);
        select.innerHTML = '';
      
        if (msg["allow-javascript"] === "false") {
            enable_javascript.classList.remove(checked);
        }
      
        const createOption = (value, text) => {
            const option = document.createElement('option');
            option.value = value; option.textContent = text;
            select.appendChild(option);
        };
      
        createOption(msg["active-skin"], msg["active-skin"]);
      
        msg.skins.forEach((skin) => {
            if (skin.name !== msg["active-skin"]) {
                createOption(skin.name, skin.name);
            }
        });
    };

    document.getElementById("openSkinPath").addEventListener("click", () => millennium_ipc({type: ipc_types.open_skins_folder}));
    document.getElementById("discordServer").addEventListener("click", () => millennium_ipc({type: ipc_types.open_url, content: "https://discord.gg/MXMWEQKgJF"}));
    document.getElementById("donate_link").addEventListener("click", () => millennium_ipc({type: ipc_types.open_url, content: "https://ko-fi.com/shadowmonster"}));
    document.getElementById("github_link").addEventListener("click", () => millennium_ipc({type: ipc_types.open_url, content: "https://github.com/ShadowMonster99"}));

    enable_javascript.addEventListener("click", () => {
        enable_javascript.classList.toggle(checked);
        const value = enable_javascript.classList.contains(checked);
        millennium_ipc({ type: ipc_types.change_javascript, content: value });
    });
}

(function millennium() 
{
    const check_panel = function(_, observer) 
    {
        const interface_tab_item = document.querySelectorAll(`[class*='pagedsettings_PagedSettingsDialog_PageListItem_']`)[5];

        if (interface_tab_item) 
        {
            interface_tab_item.addEventListener(`click`, () => 
            {
                if (interface_tab_item.classList.contains(`pagedsettings_Active_Myra7`)) {
                    return
                }

                var page_call_back = function(mutations, observe) 
                {
                    mutations.forEach(function(mutation) 
                    {
                        if (mutation.target.classList.contains("DialogContentTransition") && mutation.addedNodes.length === 1) 
                        {  
                            const container = document.createElement(`div`);
                            container.innerHTML = settings_html;
                            
                            const dialogBody = document.querySelector(`.DialogBody[class*='settings_SettingsDialogBodyFade_']`);
                            dialogBody.insertBefore(container, dialogBody.firstChild);

                            handle_interface()
                        }
                    });
                    observe.disconnect();
                };             
                new MutationObserver(page_call_back).observe(document.body, { childList: true, subtree: true });
            });    
            observer.disconnect();
        }
    }
    check_panel() // incase script injection is slow and page is loaded already
    new MutationObserver((_, observer) => check_panel(_, observer)).observe(document, { childList: true, subtree: true });
})();