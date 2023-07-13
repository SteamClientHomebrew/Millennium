console.log("millennium VM thread ->")

const repo_url = 'https://shadowmonster99.github.io/millennium-steam-patcher/root/dependencies'

const 
	millennium_html = `${repo_url}/index.html`, 
	millennium_css = `${repo_url}/styles.css`

fetch(millennium_html).then(response => response.text()).then(_data => { 
	
	const functions = {
		javascript_checkbox: (event) => {
			const js_checkbox = Object.assign(document.createElement('div'), {
				classList: ['gamepaddialog_Toggle_24G4g', 'Focusable', 
				...(event["allow-javascript"] ? ['gamepaddialog_On_3ld7T'] : [])].join(' '),
			});

			js_checkbox.appendChild(Object.assign(document.createElement('div'), { classList: ['gamepaddialog_ToggleRail_2JtC3'] }));
			js_checkbox.appendChild(Object.assign(document.createElement('div'), { classList: ['gamepaddialog_ToggleSwitch_3__OD'] }));

			document.querySelector('.gamepaddialog_FieldChildren_14_HB').appendChild(js_checkbox);

			js_checkbox.addEventListener("click", () => {
				js_checkbox.classList.toggle(js_checkbox.classList[2]);
				usermode.ipc.send({
					type: usermode.ipc_types.change_javascript, 
					content: js_checkbox.classList.contains(js_checkbox.classList[2]) 
				});
			});
		},
		populate_skins: (event) => {
			event.skins.forEach((skin) => {
				document.querySelector('.mllnm-addon-list').innerHTML += createAddonCard(
					skin.name, skin.version, skin.description, skin.author, skin.source, 
					skin.native_name === event["active-skin"] ? true : false, skin.native_name
				);
			});
		},
		populate_skins_empty: (event) => {		
			if (event.skins === null && !document.querySelector(".emptyContainer-poti7J")) {
				document.querySelector('.mllnm-addon-list').innerHTML += no_skins_available();
				functions.set_event_listeners()
			}
		},
		init_skin_change: () => {
			Array.from(document.getElementsByClassName("mllnm-switch")).forEach((element, i) => {
				element.addEventListener("click", () => {
					const is_enabled = document.querySelectorAll('.mllnm-addon-card')[i].classList[1] === "skin-enabled";
					const skin_name = document.querySelectorAll('.mllnm-addon-card .mllnm-name')[i].classList[1];

					usermode.ipc.send({
						type: usermode.ipc_types.skin_update,
						content: is_enabled ? "default" : skin_name
					});
				});
			});
		},
		init_searchbar: () => {
			const search_box = document.getElementsByClassName("mllnm-search")[0]
			const items = document.querySelectorAll('.mllnm-addon-card .mllnm-name');
			const cards = document.querySelectorAll('.mllnm-addon-card');
			
			search_box.addEventListener('input', () => {
				items.forEach((item, i) => {
					cards[i].style.display = item.innerText.toLowerCase()
						.includes(search_box.value.toLowerCase()) ? 'block' : 'none';
				});
			});
		},
		set_event_listeners: () => {
			Array.from(document.getElementsByClassName("openSkinPath")).forEach(element => element.addEventListener("click", () => {
				usermode.ipc.send({ type: usermode.ipc_types.open_skins_folder });
			}));
		},
		handle_interface: () => {
			usermode.ipc.send({type: usermode.ipc_types.get_skins})
			usermode.ipc.onmessage = function(event) {
				Object.keys(functions).slice(0, -1).forEach((key) => {
					functions[key](event);
				});
			};
		}
	}
	
	function createAddonCard(name, version, description, author, websiteLink, enabled, native_skin_name) {
		return `
		<div id="${name}-card" class="mllnm-addon-card ${enabled ? 'skin-enabled' : ''}">
			<div class="mllnm-addon-header">
				<div style="display: flex;">
					<svg viewBox="0 0 24 24" fill="#FFFFFF" class="mllnm-icon" style="width: 18px; height: 18px;">
						<path d="M0 0h24v24H0z" fill="none"></path>
						<path d="M12 3c-4.97 0-9 4.03-9 9s4.03 9 9 9c.83 0 1.5-.67 1.5-1.5 0-.39-.15-.74-.39-1.01-.23-.26-.38-.61-.38-.99 0-.83.67-1.5 1.5-1.5H16c2.76 0 5-2.24 5-5 0-4.42-4.03-8-9-8zm-5.5 9c-.83 0-1.5-.67-1.5-1.5S5.67 9 6.5 9 8 9.67 8 10.5 7.33 12 6.5 12zm3-4C8.67 8 8 7.33 8 6.5S8.67 5 9.5 5s1.5.67 1.5 1.5S10.33 8 9.5 8zm5 0c-.83 0-1.5-.67-1.5-1.5S13.67 5 14.5 5s1.5.67 1.5 1.5S15.33 8 14.5 8zm3 4c-.83 0-1.5-.67-1.5-1.5S16.67 9 17.5 9s1.5.67 1.5 1.5-.67 1.5-1.5 1.5z"></path>
					  </svg>
					  <div class="mllnm-title">
						<div class="mllnm-name ${native_skin_name}">${name}</div>
						<div class="mllnm-meta">
						  <span class="mllnm-version">${version}</span>by <span class="mllnm-author">${author}</span>
						</div>
					  </div>
				</div>
				<div class="mllnm-switch">
					<div class="${enabled ? "skin-is-active" : ""} skin-disabled active-label"></div>
				</div>
			</div>
			<div class="mllnm-footer">
				<span class="mllnm-links">
					<div class="mllnm-addon-button">
						<a class="mllnm-link mllnm-link-website" href="${websiteLink}" target="_blank" rel="noopener noreferrer">
							<svg viewBox="0 0 24 24" fill="#FFFFFF" style="width: 18px; height: 18px;">
								<path d="m12 .5c-6.63 0-12 5.28-12 11.792 0 5.211 3.438 9.63 8.205 11.188.6.111.82-.254.82-.567 0-.28-.01-1.022-.015-2.005-3.338.711-4.042-1.582-4.042-1.582-.546-1.361-1.335-1.725-1.335-1.725-1.087-.731.084-.716.084-.716 1.205.082 1.838 1.215 1.838 1.215 1.07 1.803 2.809 1.282 3.495.981.108-.763.417-1.282.76-1.577-2.665-.295-5.466-1.309-5.466-5.827 0-1.287.465-2.339 1.235-3.164-.135-.298-.54-1.497.105-3.121 0 0 1.005-.316 3.3 1.209.96-.262 1.98-.392 3-.398 1.02.006 2.04.136 3 .398 2.28-1.525 3.285-1.209 3.285-1.209.645 1.624.24 2.823.12 3.121.765.825 1.23 1.877 1.23 3.164 0 4.53-2.805 5.527-5.475 5.817.42.354.81 1.077.81 2.182 0 1.578-.015 2.846-.015 3.229 0 .309.21.678.825.56 4.801-1.548 8.236-5.97 8.236-11.173 0-6.512-5.373-11.792-12-11.792z"></path>
							</svg>
						</a>
				  	</div>
				</span>
			</div>
		</div>`;
	}
	
	function no_skins_available()
	{
		return (`
			<div class="mllnm-empty-image-container emptyContainer-poti7J">
				<div class="mllnm-empty-image emptyImage-2pCD2j"></div>
				<div class="mllnm-empty-image-header emptyHeader-2cxTFP">You don't have any themes!</div>
				<div class="mllnm-empty-image-message">Grab some from <a href="#" class="mllnm-link" target="_blank" rel="noopener noreferrer">this website</a> and add them to your theme folder. </div>
				<button class="mllnm-button openSkinPath">Open theme Folder</button>
			</div>`
		);
	}
	
	(function() 
	{
		const check_panel = function(_, observer) 
		{
			const interface_tab_item = document.querySelectorAll(`[class*='pagedsettings_PagedSettingsDialog_PageListItem_']`)[5];
			if (interface_tab_item) 
			{
				!document.querySelectorAll(`link[href='${millennium_css}']`).length && 
				document.head.appendChild(Object.assign(document.createElement('link'), { 
					rel: 'stylesheet', 
					href: millennium_css 
				}));

				interface_tab_item.addEventListener(`click`, () => 
				{
					if (interface_tab_item.classList.contains(`pagedsettings_Active_Myra7`)) {
						return
					}

					new MutationObserver(function(mutations, observe) 
					{
						mutations.forEach(function(mutation) 
						{
							if (mutation.target.classList.contains("DialogContentTransition") && mutation.addedNodes.length === 1) 
							{  
								document.querySelector(".DialogHeader").innerText = "Millennium"

								const container = document.createElement(`div`);
								container.innerHTML = _data;
								
								const dialogBody = document.querySelector(`.DialogBody[class*='settings_SettingsDialogBodyFade_']`);
								dialogBody.insertBefore(container, dialogBody.firstChild);
	
								functions.handle_interface()
							}
						});
						observe.disconnect();
					}).observe(document.body, { childList: true, subtree: true });
				});    
				observer.disconnect();
			}
		}
		check_panel()
		new MutationObserver((_, observer) => check_panel(_, observer)).observe(document, { childList: true, subtree: true });
	})()	
})