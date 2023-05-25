const socket = new WebSocket('ws://localhost:3242');

var settings_html;
fetch('modal.html').then(response => response.text()).then(_data => { settings_html = _data })

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
    const observerCallback = function(_, observer) 
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
    };

    new MutationObserver(observerCallback).observe(document, { childList: true, subtree: true });
})();
