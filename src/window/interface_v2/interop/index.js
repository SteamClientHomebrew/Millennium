import { Millennium } from "./utils/utils.js";
import { hooks } from "./hooks/tabmgr.js";
import { icons } from "./utils/icons.js";
import { stylesheets } from "./utils/style.js";
import { create_checkbox } from "./utils/checkbox.js"
import "./globals/global.js"

async function theme_tab() {

  const json = JSON.parse((await Millennium.IPC.getMessage('[get-themetab-data]')).message)

  document.querySelector(".DialogContent_InnerWidth").innerHTML = `
<div class="DialogHeader">Themes</div>
<div class="DialogBody aFxOaYcllWYkCfVYQJFs0">
  <div class="S-_LaQG5eEOM2HWZ-geJI qFXi6I-Cs0mJjTjqGXWZA _3XNvAmJ9bv_xuKx5YUkP-5 _3bMISJvxiSHPx1ol-0Aswn _3s1Rkl6cFOze_SdV2g-AFo _1ugIUbowxDg0qM0pJUbBRM _5UO-_VhgFhDWlkDIOZcn_ XRBFu6jAfd5kH9a3V8q_x wE4V6Ei2Sy2qWDo_XNcwn Panel" style="--indent-level:0;">
  <div class="H9WOq6bV_VhQ4QjJS_Bxg">
    <div class="_3b0U-QDD-uhFpw6xM716fw">Steam Skin</div>
    <div class="_2ZQ9wHACVFqZcufK_WRGPM">
      <button type="button" class="_3epr8QYWw_FqFgMx38YEEm DialogButton _DialogLayout Secondary Focusable" tabindex="0" style="margin: 0px; margin-right: 10px;" onclick="open_theme_folder()">...</button>
      <button type="button" class="_3epr8QYWw_FqFgMx38YEEm DialogButton _DialogLayout Secondary Focusable" tabindex="0" style="margin: 0px; margin-right: 10px; justify-content: center; display: flex; align-items: center; padding-left: 10px; padding-right: 10px;" onclick="edit_theme()">
        <svg width="16px" height="15px" viewBox="0 0 19 21" version="1.1" xmlns="http://www.w3.org/2000/svg" xmlns:xlink="http://www.w3.org/1999/xlink">
            <g id="Page-1" stroke="none" stroke-width="1"  fill="currentColor" fill-rule="evenodd">
                <g id="Dribbble-Light-Preview" transform="translate(-419.000000, -359.000000)" fill="#000000">
                    <g id="icons" transform="translate(56.000000, 160.000000)">
                        <path d="M384,209.210475 L384,219 L363,219 L363,199.42095 L373.5,199.42095 L373.5,201.378855 L365.1,201.378855 L365.1,217.042095 L381.9,217.042095 L381.9,209.210475 L384,209.210475 Z M370.35,209.51395 L378.7731,201.64513 L380.4048,203.643172 L371.88195,212.147332 L370.35,212.147332 L370.35,209.51395 Z M368.25,214.105237 L372.7818,214.105237 L383.18415,203.64513 L378.8298,199 L368.25,208.687714 L368.25,214.105237 Z" id="edit_cover-[#1481]"  fill="currentColor"> </path>
                    </g>
                </g>
            </g>
        </svg>
      </button>
      <div class="_3N47t_-VlHS8JAEptE5rlR">
        <div class="DialogDropDown _DialogInputContainer  Panel" tabindex="0" id="theme-dropdown" onclick="open_theme_list()">
          <div class="DialogDropDown_CurrentDisplay" id="current_theme_dialog">${json.active}</div>
          <div class="DialogDropDown_Arrow">
            <svg xmlns="http://www.w3.org/2000/svg" class="SVGIcon_Button SVGIcon_DownArrowContextMenu" data-name="Layer 1" viewBox="0 0 128 128" x="0px" y="0px">
              <polygon points="50 59.49 13.21 22.89 4.74 31.39 50 76.41 95.26 31.39 86.79 22.89 50 59.49"></polygon>
            </svg>
          </div>
        </div>
      </div>
    </div>
  </div>
  <div class="_2OJfkxlD3X9p8Ygu1vR7Lr">
    <div>Select a skin you want your Steam Client to use!</div>
    <a href="#" class="RmxP90Yut4EIwychIEg51" onclick="open_theme_store()">Browse community themes...</a>
  </div>
  </div>
  ${create_checkbox(
    "StyleSheet Insertion", json.stylesheet, 
    "Allows CSS (StyleSheet) insertions on the themes you use. CSS is always safe to use.<br><b>(Requires Restart)</b>"
  )}
  ${create_checkbox(
    "Javascript Insertion", json.javascript, 
    "Allow custom user-scripts to run in your Steam Client. JS can be malicious so only enable JS on themes you trust, or have gotten from the official community.<br><b>(Requires Restart)</b>"
  )}
  ${ navigator.userAgent.includes("Linux") ?
    `<div class="S-_LaQG5eEOM2HWZ-geJI qFXi6I-Cs0mJjTjqGXWZA _3XNvAmJ9bv_xuKx5YUkP-5 _3bMISJvxiSHPx1ol-0Aswn _3s1Rkl6cFOze_SdV2g-AFo _5UO-_VhgFhDWlkDIOZcn_ XRBFu6jAfd5kH9a3V8q_x wE4V6Ei2Sy2qWDo_XNcwn Panel" style="--indent-level:0;">
      <div class="H9WOq6bV_VhQ4QjJS_Bxg">
        <div class="_3b0U-QDD-uhFpw6xM716fw">Color Scheme</div>
        <div class="_2ZQ9wHACVFqZcufK_WRGPM">
          <button type="button" class="_3epr8QYWw_FqFgMx38YEEm DialogButton _DialogLayout Secondary Focusable" tabindex="0" style="margin: 0px; margin-right: 10px;" onclick="save_col()">Save</button><div class="_3N47t_-VlHS8JAEptE5rlR">
            <input type="color" id="favcolor" name="favcolor" value="${json.color}">
          </div>
        </div>
      </div>
      <div class="_2OJfkxlD3X9p8Ygu1vR7Lr">As Linux doesn't currently have a set-in-stone way of accessing system accent color on all distros, you have to manually set it here.</div>
    </div>`
  : ""}
  <div class="DialogSubHeader _2rK4YqGvSzXLj1bPZL8xMJ">Advanced Options</div>
  ${navigator.userAgent.includes("Linux") ? `<div class="DialogBodyText _3fPiC9QRyT5oJ6xePCVYz8">We've detected you're on Linux; the following settings do not work as intended on Linux yet.<br>Check back later...</div>` : ``}
  
  ${create_checkbox(
    "Auto-Update Themes", json.auto_update, 
    "Allow Millennium to automatically check for updates on installed themes to keep them up-to-date!"
  )}
  ${create_checkbox(
    "Auto-Update Notifications", json.update_notifs, 
    "Let Millennium push notifications when a theme is updated."
  )}
  <div class="DialogSubHeader _2rK4YqGvSzXLj1bPZL8xMJ">Client Utilities</div>
  ${navigator.userAgent.includes("Linux") ? `<div class="DialogBodyText _3fPiC9QRyT5oJ6xePCVYz8">We've detected you're on Linux; the following settings do not work, and will not work on Linux.</div>` : ``}
  
  ${create_checkbox(
    "Acrylic Drop Shadow", json.acrylic, 
    "Make steam use Windows 11 acrylic windows; Only works for supporting themes. This is currently still in development and may be unstable.<br><b>(Requires Restart & Windows 11)</b>"
  )}

  <div class="S-_LaQG5eEOM2HWZ-geJI qFXi6I-Cs0mJjTjqGXWZA _3XNvAmJ9bv_xuKx5YUkP-5 _3bMISJvxiSHPx1ol-0Aswn _3s1Rkl6cFOze_SdV2g-AFo _1ugIUbowxDg0qM0pJUbBRM _5UO-_VhgFhDWlkDIOZcn_ XRBFu6jAfd5kH9a3V8q_x wE4V6Ei2Sy2qWDo_XNcwn Panel" style="--indent-level:0;">
    <div class="H9WOq6bV_VhQ4QjJS_Bxg">
      <div class="_3b0U-QDD-uhFpw6xM716fw">Window Borders</div>
      <div class="_2ZQ9wHACVFqZcufK_WRGPM">
        <div class="_3N47t_-VlHS8JAEptE5rlR">
          <div class="DialogDropDown _DialogInputContainer  Panel" tabindex="0" id="corner-pref-dropdown" onclick="border_radius_list()">
            <div class="DialogDropDown_CurrentDisplay" id="corner-pref">${corner_prefs.find(item => item.data === json.corner_pref).name || "Unknown Position"}</div>
            <div class="DialogDropDown_Arrow">
              <svg xmlns="http://www.w3.org/2000/svg" class="SVGIcon_Button SVGIcon_DownArrowContextMenu" data-name="Layer 1" viewBox="0 0 128 128" x="0px" y="0px">
                <polygon points="50 59.49 13.21 22.89 4.74 31.39 50 76.41 95.26 31.39 86.79 22.89 50 59.49"></polygon>
              </svg>
            </div>
          </div>
        </div>
      </div>
    </div>
    <div class="_2OJfkxlD3X9p8Ygu1vR7Lr">
      <div>Custom border radius options for all the Steam windows. Currently still in development and has only been tested on Windows 11.</div>
    </div>
  </div>
  <div class="DialogSubHeader _2rK4YqGvSzXLj1bPZL8xMJ">Miscellaneous</div>

  <div class="S-_LaQG5eEOM2HWZ-geJI qFXi6I-Cs0mJjTjqGXWZA _3XNvAmJ9bv_xuKx5YUkP-5 _3bMISJvxiSHPx1ol-0Aswn _3s1Rkl6cFOze_SdV2g-AFo _1ugIUbowxDg0qM0pJUbBRM _5UO-_VhgFhDWlkDIOZcn_ XRBFu6jAfd5kH9a3V8q_x wE4V6Ei2Sy2qWDo_XNcwn Panel" style="--indent-level:0;">
    <div class="H9WOq6bV_VhQ4QjJS_Bxg">
      <div class="_3b0U-QDD-uhFpw6xM716fw">Steam Notifications</div>
      <div class="_2ZQ9wHACVFqZcufK_WRGPM">
        <div class="_3N47t_-VlHS8JAEptE5rlR">
          <div class="DialogDropDown _DialogInputContainer  Panel" tabindex="0" id="notifs-dropdown" onclick="open_notif_pos()">
            <div class="DialogDropDown_CurrentDisplay" id="notifs-pos-current">${notifications.find(item => item.data === json.notifs_pos).name || "Unknown Position"}</div>
            <div class="DialogDropDown_Arrow">
              <svg xmlns="http://www.w3.org/2000/svg" class="SVGIcon_Button SVGIcon_DownArrowContextMenu" data-name="Layer 1" viewBox="0 0 128 128" x="0px" y="0px">
                <polygon points="50 59.49 13.21 22.89 4.74 31.39 50 76.41 95.26 31.39 86.79 22.89 50 59.49"></polygon>
              </svg>
            </div>
          </div>
        </div>
      </div>
    </div>
    <div class="_2OJfkxlD3X9p8Ygu1vR7Lr">
      <div>Add a little extra spice to the positioning of Steam's notifications</div>
    </div>
  </div>

  ${create_checkbox(
    "Hide Browser URL Bar", json.url_bar, 
    "Hide the browser url bar that appears on the steam store, and other locations across the client!"
  )}
  </div>
  <div class="_2OJfkxlD3X9p8Ygu1vR7Lr desc-text-center"></div>
  <div class="_2OJfkxlD3X9p8Ygu1vR7Lr desc-text-center">Copyright &copy; Project-Millennium 2020-2024</div>
  <div class="_2OJfkxlD3X9p8Ygu1vR7Lr desc-text-center" style="margin-top: 0px;">
    <a class="RmxP90Yut4EIwychIEg51" href="https://millennium.web.app/discord" target="_blank">Discord</a>  &nbsp; &bull; &nbsp;
    <a class="RmxP90Yut4EIwychIEg51" href="https://ko-fi.com/shadowmonster" target="_blank">Donate</a>  &nbsp; &bull; &nbsp;
    <a class="RmxP90Yut4EIwychIEg51" href="https://github.com/ShadowMonster99/millennium-steam-patcher" target="_blank">Github</a>
  </div>
  <div class="_2OJfkxlD3X9p8Ygu1vR7Lr desc-text-center"></div>
  </div>
  `
}

function plugins_tab() {
  document.querySelector(".DialogContent_InnerWidth").innerHTML = `
    <div class="DialogHeader">Plugins</div>
    <div class="DialogBody aFxOaYcllWYkCfVYQJFs0">
      <div class="cdiv">
        ${icons.nothingHere}
        <div class="_2OJfkxlD3X9p8Ygu1vR7Lr desc-text-center">Coming Soon! Check back later...</div>
        <div class="_2OJfkxlD3X9p8Ygu1vR7Lr desc-text-center">Join the &nbsp;<a href="https://millennium.web.app/discord" target="_blank">discord</a>&nbsp;for progress updates</div>
      </div>
    </div>
    <div class="_2OJfkxlD3X9p8Ygu1vR7Lr desc-text-center"></div>
    <div class="_2OJfkxlD3X9p8Ygu1vR7Lr desc-text-center">Copyright &copy; Project-Millennium 2020-2024</div>
    <div class="_2OJfkxlD3X9p8Ygu1vR7Lr desc-text-center" style="margin-top: 0px;">
    <a class="RmxP90Yut4EIwychIEg51" href="https://millennium.web.app/discord" target="_blank">Discord</a>  &nbsp; &bull; &nbsp;
    <a class="RmxP90Yut4EIwychIEg51" href="https://ko-fi.com/shadowmonster" target="_blank">Donate</a>  &nbsp; &bull; &nbsp;
    <a class="RmxP90Yut4EIwychIEg51" href="https://github.com/ShadowMonster99/millennium-steam-patcher" target="_blank">Github</a>
    </div>
    <div class="_2OJfkxlD3X9p8Ygu1vR7Lr desc-text-center">
  `
}

function initialize() {
  Millennium.waitFor(".ModalPosition_Content").then(_ => {
      stylesheets.insert()
      hooks.create_seperator()
      hooks.create_tab("Themes", icons.themeTab, () => theme_tab())
      hooks.create_tab("Plugins", icons.pluginsTab, () => plugins_tab())
      hooks.start()
  })
}

//SteamClient.Browser.OpenDevTools()
initialize()