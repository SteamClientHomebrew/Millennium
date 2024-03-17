#include "settings.hpp"

std::string ui_interface::settings_page_renderer()
{
    std::string javaScript = R""""(
const positionsMap = {
  0: "Top Left",
  1: "Top Right",
  2: "Bottom Left",
  3: "Bottom Right"
};

const WaitForElement = (strSelector) => {
  return new Promise((resolve) => {
      const el = document.querySelector(strSelector);
      if (el) resolve(el);
      const pObserver = new MutationObserver(() => {
          const el = document.querySelector(strSelector);
          if (!el) return;
          resolve(el);
          pObserver.disconnect();
      });
      pObserver.observe(document.body, { subtree: true, childList: true });
  });
}


const IPC = {
  getMessage: (messageId) => {
      console.log("millennium.user.message:", JSON.stringify({id: messageId}))
      return new Promise((resolve) => {
          var originalConsoleLog = console.log;
          console.log = function(message) {
              originalConsoleLog.apply(console, arguments);
              if (message.returnId === messageId) {
                  console.log = originalConsoleLog;
                  resolve(message);
              }
          };
      });
  },
  postMessage: (messageId, contents) => {
      console.log("millennium.user.message:", JSON.stringify({id: messageId, data: contents}))
      return new Promise((resolve) => {
          var originalConsoleLog = console.log;
          console.log = function(message) {
              originalConsoleLog.apply(console, arguments);
              if (message.returnId === messageId) {
                  console.log = originalConsoleLog;
                  resolve(message);
              }
          };
      });
  }
};

let id = 0

function make_checkbox(name, enabled, description)
{
  id = id + 1
  const current = id

  window[`toggle_check${current}`] = () => {
    const result = document.getElementById(`toggle_desc_${current}`).classList.toggle('_3ld7THBuSMiFtcB_Wo165i')
    console.log("millennium.user.message:", JSON.stringify({id: `[set-${name}-status]`, value: result}))
  }

  return `
    <div class="S-_LaQG5eEOM2HWZ-geJI qFXi6I-Cs0mJjTjqGXWZA _3XNvAmJ9bv_xuKx5YUkP-5 _3bMISJvxiSHPx1ol-0Aswn _3s1Rkl6cFOze_SdV2g-AFo _5UO-_VhgFhDWlkDIOZcn_ XRBFu6jAfd5kH9a3V8q_x wE4V6Ei2Sy2qWDo_XNcwn Panel" style="--indent-level:0;">
      <div class="H9WOq6bV_VhQ4QjJS_Bxg">
        <div class="_3b0U-QDD-uhFpw6xM716fw">${name}</div>
        <div class="_2ZQ9wHACVFqZcufK_WRGPM">
          <div class="_3N47t_-VlHS8JAEptE5rlR">
            <div class="_24G4gV0rYtRbebXM44GkKk ${enabled ? "_3ld7THBuSMiFtcB_Wo165i" : ""} Focusable" id='toggle_desc_${current}' tabindex="0" onclick='toggle_check${current}()'>
              <div class="_2JtC3JSLKaOtdpAVEACsG1"></div>
              <div class="_3__ODLQXuoDAX41pQbgHf9"></div>
            </div>
          </div>
        </div>
      </div>
      <div class="_2OJfkxlD3X9p8Ygu1vR7Lr">${description}</div>
    </div>`
}

function make_css() {
  var styleElement = document.createElement('style');
  var cssText = `
  .settings_item {
    margin-bottom: 0px;
    display: flex;
    align-items: center;
    white-space: normal;
    max-width: 180px;
    text-overflow: ellipsis;
    padding: 10px 16px 10px 24px;
    font-size: 14px;
    font-weight: 400;
    color: #b8bcbf;
    text-transform: none;
    cursor: pointer;
    user-select: none;
  }
  .settings_item_icon {
    color: #b8bcbf;
    height: 20px;
    width: 20px;
    margin-right: 16px;
  }
  .settings_item_name {
    overflow: hidden;
    text-overflow: ellipsis;
  }
  .settings_item.selected {
    color: #fff;
    background-color: #3d4450;
  }
  .settings_item:not(.selected):hover {
    background-color: #dcdedf;
    color: #3d4450;
  }
  .settings_item.selected .settings_item_icon {
    color: #fff;
  }
  .settings_item:not(.selected):hover .settings_item_icon {
    color: #3d4450;
  }
  input[type="color"] {
    -webkit-appearance: none;
    border: none;
  }
  input[type="color"]::-webkit-color-swatch-wrapper {
      padding: 0;
  }
  input[type="color"]::-webkit-color-swatch {
      border: none;
  }
  input#favcolor {
      width: 35px;
      height: 100%;
      border-radius: 10px !important;
  }
  input[type="color" i] {
      padding: 0 !important;
  }
  .stop-scrolling {
    height: 100%;
    overflow: hidden;
  }
  .dialog_overlay {
    position: absolute;
    top: 0px;
    left: 0px;
    width: 100vw;
    height: 100vh;
    z-index: 99;
  }
  .cdiv {
    margin-top: -100px;
    justify-content: center;
    align-items: center;
  }
  .desc-text-center {
    align-items: center;
    justify-content: center;
    display: flex;
  }
`;
  styleElement.textContent = cssText;
  document.head.appendChild(styleElement);
}

window.save_col = () => {
  console.log("millennium.user.message:", JSON.stringify({id: `[set-color-scheme]`, value: document.querySelector("#favcolor").value}))
  window.opener.window.location.reload();
}

async function theme_tab() {

  let query = await IPC.getMessage('[get-themetab-data]')
  console.log(query)

  const json = JSON.parse(query.message)

  document.querySelector(".DialogContent_InnerWidth").innerHTML = `
  <div class="DialogContent_InnerWidth">
<div class="DialogHeader">Millennium &bull; Themes</div>
<div class="DialogBody aFxOaYcllWYkCfVYQJFs0">
  <div class="S-_LaQG5eEOM2HWZ-geJI qFXi6I-Cs0mJjTjqGXWZA _3XNvAmJ9bv_xuKx5YUkP-5 _3bMISJvxiSHPx1ol-0Aswn _3s1Rkl6cFOze_SdV2g-AFo _1ugIUbowxDg0qM0pJUbBRM _5UO-_VhgFhDWlkDIOZcn_ XRBFu6jAfd5kH9a3V8q_x wE4V6Ei2Sy2qWDo_XNcwn Panel" style="--indent-level:0;">
  <div class="H9WOq6bV_VhQ4QjJS_Bxg">
    <div class="_3b0U-QDD-uhFpw6xM716fw">Steam Skin</div>
    <div class="_2ZQ9wHACVFqZcufK_WRGPM">
      <button type="button" class="_3epr8QYWw_FqFgMx38YEEm DialogButton _DialogLayout Secondary Focusable" tabindex="0" style="margin: 0px; margin-right: 10px;" onclick="open_theme_folder()">...</button>
                                                                                                                                                                                                    <div class="_3N47t_-VlHS8JAEptE5rlR">
                                                                                                                                                                                                               <div class="DialogDropDown _DialogInputContainer  Panel" tabindex="0" id="theme-dropdown" onclick="open_theme_list()">
                                                                                                                                                                                                                                                                                                                 <div class="DialogDropDown_CurrentDisplay">${json.active}</div>
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
                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   <a href="#" class="RmxP90Yut4EIwychIEg51" onclick="open_theme_store()">Find more themes...</a>
                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               </div>
                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 </div>
                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   ${make_checkbox(
                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           "StyleSheet Insertion",
                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           json.stylesheet,
                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           "Allows CSS (StyleSheet) insertions on the themes you use. CSS is always safe to use.<br><b>(Requires Restart)</b>"
                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   )}
    ${make_checkbox(
            "Javascript Insertion",
            json.javascript,
            "Allow custom user-scripts to run in your Steam Client. JS can be malicious so only enable JS on themes you trust, or have gotten from the official community.<br><b>(Requires Restart)</b>"
    )}
    ${ navigator.userAgent.includes("Linux") &&
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
    }
    <div class="DialogSubHeader _2rK4YqGvSzXLj1bPZL8xMJ">Advanced Options</div>
                                                                  ${navigator.userAgent.includes("Linux") ? `<div class="DialogBodyText _3fPiC9QRyT5oJ6xePCVYz8">We've detected you're on Linux; the following settings do not work as intended on Linux yet.<br>Check back later...</div>` : ``}

    ${make_checkbox(
            "Auto-Update Themes",
            json.auto_update,
            "Allow Millennium to automatically check for updates on installed themes to keep them up-to-date!"
    )}
    ${make_checkbox(
            "Auto-Update Notifications",
            json.update_notifs,
            "Let Millennium push notifications when a theme is updated."
    )}
    <div class="DialogSubHeader _2rK4YqGvSzXLj1bPZL8xMJ">Miscellaneous</div>

               <div class="S-_LaQG5eEOM2HWZ-geJI qFXi6I-Cs0mJjTjqGXWZA _3XNvAmJ9bv_xuKx5YUkP-5 _3bMISJvxiSHPx1ol-0Aswn _3s1Rkl6cFOze_SdV2g-AFo _1ugIUbowxDg0qM0pJUbBRM _5UO-_VhgFhDWlkDIOZcn_ XRBFu6jAfd5kH9a3V8q_x wE4V6Ei2Sy2qWDo_XNcwn Panel" style="--indent-level:0;">
                                                                                                                                                                                                                                                       <div class="H9WOq6bV_VhQ4QjJS_Bxg">
                                                                                                                                                                                                                                                                  <div class="_3b0U-QDD-uhFpw6xM716fw">Steam Notifications</div>
                                                                                                                                                                                                                                                                                                             <div class="_2ZQ9wHACVFqZcufK_WRGPM">
                                                                                                                                                                                                                                                                                                                        <div class="_3N47t_-VlHS8JAEptE5rlR">
                                                                                                                                                                                                                                                                                                                                   <div class="DialogDropDown _DialogInputContainer  Panel" tabindex="0" id="notifs-dropdown" onclick="open_notif_pos()">
                                                                                                                                                                                                                                                                                                                                                                                                                                      <div class="DialogDropDown_CurrentDisplay" id="notifs-pos-current">${positionsMap[json.notifs_pos] || "Unknown Position"}</div>
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

                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                              ${make_checkbox(
                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      "Hide Browser URL Bar",
                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      json.url_bar,
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
    <div class="DialogContent_InnerWidth">
               <div class="DialogHeader">Millennium &bull; Plugins</div>
                                                           <div class="DialogBody aFxOaYcllWYkCfVYQJFs0 cdiv">
                                                                      <div>
                                                                      <svg xmlns="http://www.w3.org/2000/svg" xmlns:xlink="http://www.w3.org/1999/xlink" fill="none" height="110" viewBox="0 0 240 110" width="240"><linearGradient id="a" gradientUnits="userSpaceOnUse" x1="39.567" x2="39.567" y1="39.2948" y2="101.804"><stop offset="0" stop-color="#72767d"/><stop offset="1" stop-color="#fff" stop-opacity="0"/></linearGradient><clipPath id="b"><path d="m0 0h240v110h-240z"/></clipPath><g clip-path="url(#b)"><path d="m0 109.243v-5.607c0-2.272.917198-4.3935 2.52229-5.9844l11.77071-11.6667c2.0637-2.0454 3.2102-4.7727 3.2102-7.6515v-14.5454c0-6.6667 5.5032-12.1213 12.2293-12.1213h169.5285c6.726 0 12.229 5.4546 12.229 12.1213v27.2727c0 2.5 2.064 4.5454 4.586 4.5454h11.542c5.732 0 10.318 4.6209 10.318 10.2269v3.41z" fill="#72767d" opacity=".2"/><path d="m83.465 56.2122h-22.1656v53.0298h22.1656z" fill="#36393f"/><path d="m193.529 56.2122h-110.064v53.0298h110.064z" fill="#4f545c"/><path d="m106.395 78.9395h-22.93v30.3025h22.93z" fill="#b9bbbe"/><path d="m109.452 74.3938h-25.987v4.5455h25.987z" fill="#dcddde"/><path d="m83.465 74.3938h-14.5223v4.5455h14.5223z" fill="#72767d"/><path d="m103.338 66.8181h-19.873v7.5758h19.873z" fill="#f6f6f7"/><path d="m83.465 66.8181h-6.879v7.5758h6.879z" fill="#b9bbbe"/><path d="m102.573 82.7273h-15.2864v1.5151h15.2864z" fill="#72767d"/><path d="m102.573 84.2424h-15.2864v1.5152h15.2864z" fill="#4f545c"/><path d="m102.573 85.7576h-15.2864v1.5151h15.2864z" fill="#72767d"/><path d="m102.573 87.2727h-15.2864v1.5152h15.2864z" fill="#4f545c"/><path d="m102.573 88.7878h-15.2864v1.5152h15.2864z" fill="#72767d"/><path d="m102.573 90.303h-15.2864v1.5151h15.2864z" fill="#4f545c"/><path d="m102.573 91.8181h-15.2864v1.5152h15.2864z" fill="#72767d"/><path d="m102.573 93.3333h-15.2864v1.5151h15.2864z" fill="#4f545c"/><path d="m104.102 94.8484h-18.344v3.0303h18.344z" fill="#dcddde"/><path d="m83.465 78.9395h-12.2293v30.3025h12.2293z" fill="#202225"/><path d="m75.0573 94.8485h4.586v-9.8485c0-1.2879-.9936-2.2727-2.293-2.2727s-2.293.9848-2.293 2.2727z" fill="#36393f"/><path d="m185.885 74.394h-16.815v25.758h16.815z" fill="#72767d"/><path d="m183.592 76.6667h-12.229v21.2122h12.229z" fill="#b9bbbe"/><path d="m115.524 109.214h45.86v-37.8789h-45.86z" fill="#72767d"/><path d="m118.582 109.214h39.745v-34.8486h-39.745z" fill="#040405"/><path d="m140.025 73.6362h-3.057v35.6058h3.057z" fill="#72767d"/><path d="m144.611 92.1968h-12.229v6.0606h12.229z" fill="#72767d"/><path d="m120.153 26.6667h-36.688c-5.0446 0-9.172 4.091-9.172 9.091v9.0909h45.86z" fill="#72767d"/><path d="m111.745 26.6667h57.325c5.045 0 9.172 4.091 9.172 9.091v9.0909h-75.669v-9.0909c0-5 4.051-9.091 9.172-9.091z" fill="#b9bbbe"/><path d="m105.631 44.8485v-9.0909c0-3.3333 2.751-6.0606 6.114-6.0606h57.325c3.363 0 6.115 2.7273 6.115 6.0606v9.0909z" fill="#72767d"/><path d="m196.586 38.0303h-113.121v21.9697h113.121z" fill="#dcddde"/><path d="m83.4224 56.9412h110.0636v-3.7879h-110.0636z" fill="#f6f6f7"/><path d="m193.529 47.1213h-110.064v3.7879h110.064z" fill="#f6f6f7"/><path d="m83.4224 44.8201h110.0636v-3.7879h-110.0636z" fill="#f6f6f7"/><path d="m83.465 38.0303h-25.2229v21.9697h25.2229z" fill="#b9bbbe"/><path d="m61.2568 56.9412h22.1656v-3.7879h-22.1656z" fill="#dcddde"/><path d="m61.2568 50.8806h22.1656v-3.7879h-22.1656z" fill="#dcddde"/><path d="m61.2568 44.8201h22.1656v-3.7879h-22.1656z" fill="#dcddde"/><path d="m209.885 2.12121c0-.68182.077-1.439392.23-2.12121-5.351.984848-9.478 5.68182-9.478 11.2879 0 6.3636 5.197 11.4394 11.541 11.4394 5.656 0 10.395-4.0152 11.389-9.394-.688.1515-1.376.2273-2.14.2273-6.421 0-11.542-5.15151-11.542-11.43939z" fill="#b9bbbe"/><path d="m6.19112 73.8636-.99363 1.9697c-.2293.4546.2293.9849.76433.7576l1.98726-.9848c.15287-.0758.30573-.0758.4586 0l1.98722.9848c.4586.2273.9937-.2273.7644-.7576l-.9937-1.9697c-.0764-.1515-.0764-.303 0-.4545l.9937-1.9697c.2293-.4546-.2293-.9849-.7644-.7576l-1.98722.9849c-.15287.0757-.30573.0757-.4586 0l-1.98726-.9849c-.4586-.2273-.99363.2273-.76433.7576l.99363 1.9697c.07643.1515.07643.303 0 .4545z" fill="#b9bbbe"/><path d="m191.465 30.3031-.994 1.9697c-.229.4545.23.9848.765.7576l1.987-.9849c.153-.0757.306-.0757.458 0l1.988.9849c.458.2272.993-.2273.764-.7576l-.994-1.9697c-.076-.1515-.076-.303 0-.4546l.994-1.9697c.229-.4545-.229-.9848-.764-.7575l-1.988.9848c-.152.0758-.305.0758-.458 0l-1.987-.9848c-.459-.2273-.994.2272-.765.7575l.994 1.9697c.076.1516.076.3031 0 .4546z" fill="#4f545c"/><path d="m235.796 26.2879c-.458 0-.764-.3031-.764-.7576v-.7576c0-.4545.306-.7576.764-.7576.459 0 .764.3031.764.7576v.7576c0 .3788-.382.7576-.764.7576z" fill="#dcddde"/><path d="m235.796 32.3484c-.458 0-.764-.303-.764-.7576v-.7575c0-.4546.306-.7576.764-.7576.459 0 .764.303.764.7576v.7575c0 .3788-.382.7576-.764.7576z" fill="#dcddde"/><path d="m237.707 28.1819c0-.4546.306-.7576.764-.7576h.765c.458 0 .764.303.764.7576 0 .4545-.306.7576-.764.7576h-.765c-.458 0-.764-.3788-.764-.7576z" fill="#dcddde"/><path d="m231.592 28.1819c0-.4546.306-.7576.765-.7576h.764c.459 0 .764.303.764.7576 0 .4545-.305.7576-.764.7576h-.764c-.459 0-.765-.3788-.765-.7576z" fill="#dcddde"/><g fill="#040505"><path d="m37.2229 93.106c0-1.4394-1.07-2.803-2.5223-3.0303-1.8344-.303-3.4395 1.1363-3.4395 2.8788 0 1.5909-.3057 3.2575-1.1465 4.6212-.3821.6818-.6114 1.5151-.6114 2.3485.0764 2.4238 2.0637 4.3938 4.4331 4.5458 2.6752.151 4.8917-1.97 4.8917-4.6216 0-.8333-.2293-1.5909-.6114-2.197-.6879-1.3636-.9937-2.9545-.9937-4.5454z"/><path d="m32.1783 110c-.3822 0-.6879-.303-.6879-.682v-8.333c0-.379.3057-.682.6879-.682s.6879.303.6879.682v8.333c0 .379-.3057.682-.6879.682z"/><path d="m36.3057 110c-.3822 0-.6879-.303-.6879-.682v-8.333c0-.379.3057-.682.6879-.682s.6879.303.6879.682v8.333c-.0764.379-.3822.682-.6879.682z"/><path d="m182.752 93.106c0-1.4394-1.07-2.803-2.523-3.0303-1.834-.303-3.439 1.1363-3.439 2.8788 0 1.5909-.306 3.2575-1.147 4.6212-.382.6818-.611 1.5151-.611 2.3485.076 2.4238 2.064 4.3938 4.433 4.5458 2.675.151 4.892-1.97 4.892-4.6216 0-.8333-.23-1.5909-.612-2.197-.688-1.3636-.993-2.9545-.993-4.5454z"/><path d="m177.707 110c-.382 0-.688-.303-.688-.682v-8.333c0-.379.306-.682.688-.682s.688.303.688.682v8.333c0 .379-.306.682-.688.682z"/><path d="m181.834 110c-.382 0-.688-.303-.688-.682v-8.333c0-.379.306-.682.688-.682.383 0 .688.303.688.682v8.333c-.076.379-.382.682-.688.682z"/></g><path d="m16.6624 109.242h45.8599l-18.344-65.1512h-9.1719z" fill="url(#a)"/><path d="m32.7134 44.0909h13.7579v-1.5152c0-1.6667-1.3758-3.0303-3.0573-3.0303h-7.6433c-1.6815 0-3.0573 1.3636-3.0573 3.0303z" fill="#dcddde"/><path d="m39.5923 41.8181v-8.7878c0-3.3334 2.6752-5.9849 6.0383-5.9849 3.363 0 6.0382 2.6515 6.0382 5.9849v76.2117" stroke="#dcddde" stroke-miterlimit="10" stroke-width="2"/></g><style xmlns="" class="darkreader darkreader--fallback"></svg>
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
                                                                                                                 <div class="_2OJfkxlD3X9p8Ygu1vR7Lr desc-text-center"></div>
                                                                                                                                                                         </div>
    `
}

function make_tab(name, icon) {
    return `<div class="settings_item " id="${name}">
                                           <div class="U6HcKswXzjmWtFxbjxuz4 settings_item_icon">
                                                      ${icon}
                                                      </div>
                                                        <div class="_2X9_IsQsEJDpAd2JGrHdJI settings_item_name">${name}</div>
                                                                                                                         </div>`
}

window.open_theme_store = () => {
window.opener.SteamUIStore.Navigate(
'/browser',
window.opener.MainWindowBrowserManager.LoadURL('https://millennium.web.app/themes')
)
}
window.open_theme_folder = () => {
console.log("millennium.user.message:", JSON.stringify({id: '[open-theme-folder]'}))
}

window.open_notif_pos = async () => {

document.querySelectorAll("._2qyBZV8YvxstXuSKiYDF19").forEach(el => el.remove())

console.log("opening")

let query = await IPC.getMessage('[get-theme-list]')
let message = JSON.parse(query.message)

console.log(message)


const el = document.getElementById("notifs-dropdown")
const rect = el.getBoundingClientRect();

let menu = `
<div class="dialog_overlay"></div>
<div class="DialogMenuPosition visible _2qyBZV8YvxstXuSKiYDF19" tabindex="0" style="position: absolute; visibility: visible; top: ${rect.top}px; right: 26px; height: fit-content;">
                                                                                   <div class="_1tiuYeMmTc9DMG4mhgaQ5w _DialogInputContainer" id="millennium-dropdown">
                                                                                                                                                 <div class="_1R-DVEa2yqX0no8BYLtn9N notifposc" pos="0">Top Left</div>
<div class="_1R-DVEa2yqX0no8BYLtn9N notifposc" pos="1">Top Right</div>
<div class="_1R-DVEa2yqX0no8BYLtn9N notifposc" pos="2">Bottom Left</div>
<div class="_1R-DVEa2yqX0no8BYLtn9N notifposc" pos="3">Bottom Right</div>
</div>
</div>
`;

document.body.insertAdjacentHTML('afterbegin', menu);

const dialog = document.querySelector("._2qyBZV8YvxstXuSKiYDF19")

dialog.style.top = rect.top - dialog.clientHeight + "px"
dialog.focus()

document.querySelectorAll('.notifposc').forEach(item => {
item.addEventListener('click', function(event) {

        console.log("millennium.user.message:", JSON.stringify({id: `[set-notifs-pos]`, value: parseInt(this.getAttribute('pos'))}))
        document.getElementById("notifs-pos-current").innerText = positionsMap[parseInt(this.getAttribute('pos'))]
});
});
}

window.open_theme_list = async () => {
document.querySelectorAll("._2qyBZV8YvxstXuSKiYDF19").forEach(el => el.remove())

console.log("opening")

let query = await IPC.getMessage('[get-theme-list]')
let message = JSON.parse(query.message)

console.log(message)

const el = document.getElementById("theme-dropdown")
const rect = el.getBoundingClientRect();

var dialogMenu = `
<div class="dialog_overlay"></div>
<div class="DialogMenuPosition visible _2qyBZV8YvxstXuSKiYDF19" tabindex="0" style="position: absolute; visibility: visible; top: ${rect.bottom}px; right: 26px; height: fit-content;">
                                                                                   <div class="_1tiuYeMmTc9DMG4mhgaQ5w _DialogInputContainer" id="millennium-dropdown">
                                                                                                                                                 <div class="_1R-DVEa2yqX0no8BYLtn9N">< default skin ></div>
`;
for (var i = 0; i < message.length; i++) {
dialogMenu += `
<div class="_1R-DVEa2yqX0no8BYLtn9N" native-name="${message[i]["native-name"]}">${message[i].name}</div>
`;
}
dialogMenu += `
</div>
</div>
`;

document.body.insertAdjacentHTML('afterbegin', dialogMenu);

document.querySelector("._2qyBZV8YvxstXuSKiYDF19").focus()

document.querySelectorAll('._1R-DVEa2yqX0no8BYLtn9N').forEach(item => {
item.addEventListener('click', function(event) {

        console.log('changing theme')
        IPC.postMessage("[update-theme-select]", this.getAttribute('native-name'));
});
});

// Add event listener to "< default skin >" item
document.querySelector('._1R-DVEa2yqX0no8BYLtn9N:first-child').addEventListener('click', function(event) {
        IPC.postMessage("[update-theme-select]", 'default');
});
}

document.body.addEventListener('click', function(event) {
        if (!event.target.classList.contains("_1tiuYeMmTc9DMG4mhgaQ5w")) {
            try {
                const element = document.querySelector("._1tiuYeMmTc9DMG4mhgaQ5w")

                if (element.id == "millennium-dropdown") {
                    element.remove()
                    document.querySelectorAll(".dialog_overlay").forEach(el => {
                            el.remove();
                    })
                }
            } catch (error) {}
        }
});

function initialize() {
    WaitForElement(".PopupFullWindow").then(element => {

            make_css()

            const buffer = document.createElement("div");
            buffer.innerHTML = `<div class="_1UEEmNDZ7Ta3enwTf5T0O0 " id="topsep"></div>`
            document.querySelector("._EebF_xe4DGRZ9a0XkyDj").prepend(buffer.firstChild)

            buffer.innerHTML = make_tab("Themes", `<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 48 48">
            <g id="_21_-_30" data-name="21 - 30">
            <g id="Art">
            <path d="M45.936,18.9a23.027,23.027,0,0,0-1.082-2.1L39.748,30.67a4.783,4.783,0,0,1-.837,1.42,8.943,8.943,0,0,0,7.464-12.115C46.239,19.609,46.093,19.253,45.936,18.9Z" fill="currentColor"/>
            <path d="M16.63,6.4A23.508,23.508,0,0,0,2.683,37.268c.031.063.052.125.083.188a8.935,8.935,0,0,0,15.662,1.526A16.713,16.713,0,0,1,26.165,32.7c.1-.04.2-.07.3-.107a6.186,6.186,0,0,1,3.859-3.453,4.865,4.865,0,0,1,.451-2.184l7.9-17.107A23.554,23.554,0,0,0,16.63,6.4ZM10.5,32.5a4,4,0,1,1,4-4A4,4,0,0,1,10.5,32.5Zm5-11.5a4,4,0,1,1,4-4A4,4,0,0,1,15.5,21Zm12-3.5a4,4,0,1,1,4-4A4,4,0,0,1,27.5,17.5Z" fill="currentColor"/>
            <path d="M45.478,4.151a1.858,1.858,0,0,0-2.4.938L32.594,27.794a2.857,2.857,0,0,0,.535,3.18,4.224,4.224,0,0,0-4.865,2.491c-1.619,3.91.942,5.625-.678,9.535a10.526,10.526,0,0,0,8.5-6.3,4.219,4.219,0,0,0-1.25-4.887,2.85,2.85,0,0,0,3.037-1.837l8.64-23.471A1.859,1.859,0,0,0,45.478,4.151Z" fill="currentColor"/>
            </g>
            </g>
            </svg>`);
            document.querySelector("._EebF_xe4DGRZ9a0XkyDj").prepend(buffer.firstChild)

            document.getElementById("Themes").addEventListener('click', async (e) => {
                theme_tab()
                document.querySelectorAll('.bkfjn0yka2uHNqEvWZaTJ').forEach(element => {
                        element.classList.remove("Myra7iGjzCdMPzitboVfh")
                })

                document.getElementById("Plugins").classList.remove("selected")
                document.getElementById("Themes").classList.add("selected")
            })

            buffer.innerHTML = make_tab("Plugins", `<svg version="1.1" id="Icons" xmlns="http://www.w3.org/2000/svg" xmlns:xlink="http://www.w3.org/1999/xlink" x="0px" y="0px" viewBox="0 0 32 32" style="enable-background:new 0 0 32 32;" xml:space="preserve">
            <g>
            <path d="M18.3,17.3L15,20.6L11.4,17l3.3-3.3c0.4-0.4,0.4-1,0-1.4s-1-0.4-1.4,0L10,15.6l-1.3-1.3c-0.4-0.4-1-0.4-1.4,0s-0.4,1,0,1.4
            L7.6,16l-2.8,2.8C3.6,19.9,3,21.4,3,23c0,1.3,0.4,2.4,1.1,3.5l-2.8,2.8c-0.4,0.4-0.4,1,0,1.4C1.5,30.9,1.7,31,2,31s0.5-0.1,0.7-0.3
            l2.8-2.8C6.5,28.6,7.7,29,9,29c1.6,0,3.1-0.6,4.2-1.7l2.8-2.8l0.3,0.3c0.2,0.2,0.5,0.3,0.7,0.3s0.5-0.1,0.7-0.3
            c0.4-0.4,0.4-1,0-1.4L16.4,22l3.3-3.3c0.4-0.4,0.4-1,0-1.4S18.7,16.9,18.3,17.3z"  fill="currentColor"></path>
            <path d="M30.7,1.3c-0.4-0.4-1-0.4-1.4,0l-2.8,2.8C25.5,3.4,24.3,3,23,3c-1.6,0-3.1,0.6-4.2,1.7l-3.5,3.5c-0.4,0.4-0.4,1,0,1.4l7,7
            c0.2,0.2,0.5,0.3,0.7,0.3s0.5-0.1,0.7-0.3l3.5-3.5C28.4,12.1,29,10.6,29,9c0-1.3-0.4-2.4-1.1-3.5l2.8-2.8
            C31.1,2.3,31.1,1.7,30.7,1.3z"  fill="currentColor"></path>
            </g>
            </svg>`);
            document.querySelector("._EebF_xe4DGRZ9a0XkyDj").prepend(buffer.firstChild)

            document.getElementById("Plugins").addEventListener('click', (e) => {
                plugins_tab()
                document.querySelectorAll('.bkfjn0yka2uHNqEvWZaTJ').forEach(element => {
                        element.classList.remove("Myra7iGjzCdMPzitboVfh")
                })
                document.getElementById("Themes").classList.remove("selected")
                document.getElementById("Plugins").classList.add("selected")
            })

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
    })
}

SteamClient.Browser.OpenDevTools()
initialize()
)"""";


    return javaScript;
}