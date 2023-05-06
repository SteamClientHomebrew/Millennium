console.log("starting settings inject");

const intervalId = setInterval(insertMillennium, 1);
var set = false

function insertMillennium()
{
	if (document.querySelector('.DialogHeader').innerText != "Interface") {
		set = false;
		return;
	}

	if (set == false)
	{
        fetch('https://steamloopback.host/skins/millennium/settings.html')
        .then(response => response.text())
        .then(data => {
            const container = document.createElement('div');
            container.innerHTML = data;
        
            const dialogBody = document.querySelector('.DialogBody.settings_SettingsDialogBodyFade_aFxOa');
            const firstChild = dialogBody.firstChild;
            dialogBody.insertBefore(container, firstChild);
            
            fetch('https://steamloopback.host/skins/settings.json')
            .then(response => response.json())
            .then(data => { 
                const skins = data.skins;
                const select = document.getElementById("skinsSelect");
                {
                    const option = document.createElement('option');
                    option.value = data["active-skin"];
                    option.textContent = data["active-skin"];
                    select.appendChild(option);
                }

                skins.forEach((skin) => {
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
            })
        });
	}
    
    set = true;
}
