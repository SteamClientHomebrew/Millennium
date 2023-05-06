const intervalId = setInterval(insertMillennium, 1);
var set = false
var settings_html;
var data;

fetch('https://raw.githubusercontent.com/ShadowMonster99/millennium-steam-patcher/main/helpers/settings.html')
.then(response => response.text())
.then(_data => { settings_html = _data })

fetch('https://steamloopback.host/skins/settings.json')
.then(response => response.json())
.then(_data => { data = _data })

function insertMillennium()
{
	if (document.querySelector('.DialogHeader').innerText != "Interface") {
		set = false;
		return;
	}

	if (set == false)
	{
        set = true;

        const container = document.createElement('div');
        container.innerHTML = settings_html;
    
        const dialogBody = document.querySelector('.DialogBody.settings_SettingsDialogBodyFade_aFxOa');
        const firstChild = dialogBody.firstChild;
        dialogBody.insertBefore(container, firstChild);

        const select = document.getElementById("skinsSelect");
        {
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
            const selectedOption = event.target.value;
            console.log("Selected skin: ", selectedOption);

            fetch('http://localhost:6438', {
                method: 'POST',
                body: selectedOption
            })
        });
	}
}