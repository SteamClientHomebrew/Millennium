let tab_store = []

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

        document.querySelector("._EebF_xe4DGRZ9a0XkyDj").insertAdjacentHTML('afterbegin', `
        <div class="settings_item " id="${name}">
            <div class="U6HcKswXzjmWtFxbjxuz4 settings_item_icon">${icon}</div>
            <div class="_2X9_IsQsEJDpAd2JGrHdJI settings_item_name">${name}</div>
        </div>`)

        tab_store.push(name)

        document.getElementById(name).addEventListener('click', async (e) => {
            callback()

            // handle unselecting other tabs
            document.querySelectorAll('.bkfjn0yka2uHNqEvWZaTJ').forEach(element => { element.classList.remove("Myra7iGjzCdMPzitboVfh")})
            tab_store.forEach(tab => tab != name && document.getElementById(tab).classList.remove("selected"))
            document.getElementById(name).classList.add("selected")
        })
    },
    create_seperator: () => {
        document.querySelector("._EebF_xe4DGRZ9a0XkyDj").insertAdjacentHTML('afterbegin', `<div class="_1UEEmNDZ7Ta3enwTf5T0O0 " id="topsep"></div>`);
    }
}

export { hooks }