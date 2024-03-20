import { modal } from "../hooks/modals.js";

const combo = {
    show: async (dropdown_el, items, cb) => {
        modal.remove_all()
        // preliminary wait, not sure why its needed, but doesnt work otherwise
        await new Promise(resolve => setTimeout(resolve, 100));

        document.body.insertAdjacentHTML('afterbegin', `
            <div class="dialog_overlay"></div>
            <div class="DialogMenuPosition visible _2qyBZV8YvxstXuSKiYDF19" tabindex="0" style="position: absolute; visibility: visible; height: fit-content;">
                <div class="_1tiuYeMmTc9DMG4mhgaQ5w _DialogInputContainer" id="millennium-dropdown">
                    ${items.map(item => `<div class="_1R-DVEa2yqX0no8BYLtn9N" data="${item.data}">${item.name}</div>`).join('')}
                </div>
            </div>
        `);

        modal.set_position(dropdown_el.getBoundingClientRect())

        document.querySelectorAll('._1tiuYeMmTc9DMG4mhgaQ5w').forEach(item => {
            item.addEventListener('click', function(event) {
                cb(event)
            });
        });
    }
}

export { combo }