const socket = new WebSocket('ws://localhost:3242');

var settings_html, sidebar_html;
var data;

fetch('https://raw.githubusercontent.com/ShadowMonster99/millennium-steam-patcher/main/settings_modal/index.html').then(response => response.text()).then(_data => { settings_html = _data })
fetch('https://raw.githubusercontent.com/ShadowMonster99/millennium-steam-patcher/main/settings_modal/sidebar.html').then(response => response.text()).then(_data => { sidebar_html = _data })
fetch('https://steamloopback.host/skins/settings.json').then(response => response.json()).then(_data => { data = _data })

function millennium_ipc(value) { socket.send(JSON.stringify(value)) }

function load_page()
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

    document.getElementById("openSkinPath").addEventListener("click", () => millennium_ipc({type: 0}));

    select.addEventListener("change", (event) => {
        const value = event.target.value;
        millennium_ipc({type: 1, content: value})
    });

    document.getElementById("discordServer").addEventListener("click", () => millennium_ipc({type: 2, content: "https://discord.gg/MXMWEQKgJF"}));
    document.getElementById("donate_link").addEventListener("click", () => millennium_ipc({type: 6, content: "my paypal is purchase-millennium@outlook.com\ndont send anything more than a couple dollars if you decide to send something.\n\nthanks for helping support the project! :)"}));
    document.getElementById("github_link").addEventListener("click", () => millennium_ipc({type: 2, content: "https://github.com/ShadowMonster99"}));
    document.getElementById("uninstall_millennium").addEventListener("click", () => millennium_ipc({type: 5}));

    const enable_javascript = document.querySelector("#enable_js");

    if (!data["allow-javascript"])
    {
        enable_javascript.classList.remove("gamepaddialog_On_3ld7T");
    }

    enable_javascript.addEventListener("click", () => 
    {
        if (enable_javascript.classList.contains("gamepaddialog_On_3ld7T")){ enable_javascript.classList.remove("gamepaddialog_On_3ld7T"); value = false; }
        else { enable_javascript.classList.add("gamepaddialog_On_3ld7T"); value = true; }

        millennium_ipc({type: 3, content: value})
    });

    const enable_console = document.querySelector("#enable_console");

    if (!data["enable-console"])
    {
        enable_console.classList.remove("gamepaddialog_On_3ld7T");
    }

    enable_console.addEventListener("click", () => 
    {
        if (enable_console.classList.contains("gamepaddialog_On_3ld7T")){ enable_console.classList.remove("gamepaddialog_On_3ld7T"); value = false; }
        else { enable_console.classList.add("gamepaddialog_On_3ld7T"); value = true; }

        millennium_ipc({type: 4, content: value})
    });
}

function bootstrap_millennium()
{
    const dialogBody = document.querySelector("[class*='pagedsettings_PagedSettingsDialog_PageList__']");

    if (dialogBody != null && sidebar_html != "undefined" && sidebar_html != null)
    {
        const container = document.createElement('div');
        container.innerHTML = sidebar_html;    

        console.log({"sidebar": sidebar_html, "query": dialogBody})

        const firstChild = dialogBody.firstChild;
        dialogBody.insertBefore(container, firstChild);

        const millenniumTab = document.getElementById('millennium_tab');
        millenniumTab.addEventListener('click', function() {
            console.log("click");
            document.getElementsByClassName("DialogContent_InnerWidth")[0].innerHTML = settings_html;
            load_page()
        });

        clearInterval(val)
    }

} const val = setInterval(bootstrap_millennium, 1);