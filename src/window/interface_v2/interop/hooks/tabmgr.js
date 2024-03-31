let tab_store = []

function get_listbox() {
    const ret = document.querySelector("._EebF_xe4DGRZ9a0XkyDj")

    if (ret != null) {
        return ret
    }
    else {
        return document.querySelector(".pagedsettings_PagedSettingsDialog_PageList__EebF")
    }
}

const hooks = {
    start: () => {
        const elements = document.querySelectorAll('.bkfjn0yka2uHNqEvWZaTJ');
        let is_processing = false;
  
        elements.forEach((element, index) => {
            element.addEventListener('click', function(e) {
  
                if (is_processing) return
                document.querySelectorAll('.settings_item').forEach(element => element.classList.remove("selected"))
                document.querySelectorAll('._1UEEmNDZ7Ta3enwTf5T0O0').forEach(element => element.classList.remove("SeoUZ6M01FoetLA2uCUtT"))
                
                const ci = index + 1
                const click = new MouseEvent("click", { view: window, bubbles: true, cancelable: true })
                try {
                  is_processing = true;
                  if (index + 1 <= elements.length) elements[index + 1].dispatchEvent(click);
                  else elements[index - 1].dispatchEvent(click);
                  
                  elements[index].dispatchEvent(click);
                  is_processing = false;
                }
                catch (error) { console.log(error) }
            });
        })
    },
    create_tab: (name, icon, callback) => {

        get_listbox().insertAdjacentHTML('afterbegin', `
        <div class="settings_item " id="${name}">
            <div class="U6HcKswXzjmWtFxbjxuz4 pagedsettings_PageListItem_Icon_U6HcK settings_item_icon">${icon}</div>
            <div class="_2X9_IsQsEJDpAd2JGrHdJI pagedsettings_PageListItem_Title_2X9_I settings_item_name">${name}</div>
        </div>`)

        tab_store.push(name)

        document.getElementById(name).addEventListener('click', async (e) => {
            callback()

            // handle unselecting other tabs
            document.querySelectorAll('.bkfjn0yka2uHNqEvWZaTJ').forEach(element => { element.classList.remove("Myra7iGjzCdMPzitboVfh")})
            document.querySelectorAll('.pagedsettings_PagedSettingsDialog_PageListItem_bkfjn').forEach(element => { element.classList.remove("pagedsettings_Active_Myra7")})

            tab_store.forEach(tab => tab != name && document.getElementById(tab).classList.remove("selected"))
            document.getElementById(name).classList.add("selected")
        })
    },
    create_seperator: () => {
        get_listbox().insertAdjacentHTML('afterbegin', `<div class="_1UEEmNDZ7Ta3enwTf5T0O0 pagedsettings_PageListSeparator_1UEEm" id="topsep"></div>`);
    }
}

export { hooks }