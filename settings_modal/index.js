const socket = new WebSocket('ws://localhost:3242');

var settings_html, sidebar_html, data;

fetch('https://raw.githubusercontent.com/ShadowMonster99/millennium-steam-patcher/main/settings_modal/index.html').then(response => response.text()).then(_data => { settings_html = _data })
fetch('https://steamloopback.host/skins/settings.json').then(response => response.json()).then(_data => { data = _data })

function millennium_ipc(value) { socket.send(JSON.stringify(value)) }

function handle_interface()
{
    const select = document.getElementById("skinsSelect"); {
        const option = document.createElement('option');
        option.value = data["active-skin"];
        option.textContent = data["active-skin"];
        select.appendChild(option);
    }

    data.skins.forEach((skin) => {
        if (skin.name != data["active-skin"])
        {
            const option = document.createElement('option');
            option.value = skin.name;
            option.textContent = skin.name;
            select.appendChild(option);
        }
    });

    select.addEventListener("change", (event) => {
        const value = event.target.value;
        millennium_ipc({type: 1, content: value})
    });

    document.getElementById("openSkinPath").addEventListener("click", () => millennium_ipc({type: 0}));
    document.getElementById("discordServer").addEventListener("click", () => millennium_ipc({type: 2, content: "https://discord.gg/MXMWEQKgJF"}));
    document.getElementById("donate_link").addEventListener("click", () => millennium_ipc({type: 2, content: "https://ko-fi.com/shadowmonster"}));
    document.getElementById("github_link").addEventListener("click", () => millennium_ipc({type: 2, content: "https://github.com/ShadowMonster99"}));

    const checked = "gamepaddialog_On_3ld7T"

    //handle enable javascript events
    {
        const enable_javascript = document.querySelector("#enable_js");

        if (!data["allow-javascript"])
            enable_javascript.classList.remove(checked);

        enable_javascript.addEventListener("click", () => 
        {
            if (enable_javascript.classList.contains(checked)) { 
                enable_javascript.classList.remove(checked); 
                value = false; 
            }
            else { 
                enable_javascript.classList.add(checked); 
                value = true; 
            }

            millennium_ipc({type: 3, content: value})
        });
    }

    //handle enable console events
    {
        const enable_console = document.querySelector("#enable_console");

        if (!data["enable-console"])
            enable_console.classList.remove(checked);

        enable_console.addEventListener("click", () => 
        {
            if (enable_console.classList.contains(checked)) { 
                enable_console.classList.remove(checked); 
                value = false; 
            }
            else { 
                enable_console.classList.add(checked); 
                value = true; 
            }

            millennium_ipc({type: 4, content: value})
        });
    }
}

const intervalId = setInterval(() => {
    const tab_name = document.querySelector('.DialogHeader');

    if (tab_name.innerText == localizedStringInterface) {

        if (set) return

        set = true;

        const container = document.createElement('div');
        container.innerHTML = settings_html;
        
        const dialogBody = document.querySelector('.DialogBody.settings_SettingsDialogBodyFade_aFxOa');
        const firstChild = dialogBody.firstChild;
        dialogBody.insertBefore(container, firstChild);

        handle_interface()
    }
    else set = false;
}, 1);