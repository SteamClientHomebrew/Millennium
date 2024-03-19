#include <stdafx.h>
#include "editor.hpp"
#include <utils/config/config.hpp>
#include <core/injector/event_handler.hpp>
#include <core/steam/cef_manager.hpp>
#include <core/injector/conditions/conditionals.hpp>
#include <window/interface/globals.h>

const std::string get_button_description(std::string title, std::string description, std::string buttonText) {
    return R"(
        <div class="S-_LaQG5eEOM2HWZ-geJI qFXi6I-Cs0mJjTjqGXWZA _3XNvAmJ9bv_xuKx5YUkP-5 _3bMISJvxiSHPx1ol-0Aswn _3s1Rkl6cFOze_SdV2g-AFo _1ugIUbowxDg0qM0pJUbBRM _5UO-_VhgFhDWlkDIOZcn_ XRBFu6jAfd5kH9a3V8q_x wE4V6Ei2Sy2qWDo_XNcwn Panel" tabindex="-1" style="--indent-level:0">
            <div class="H9WOq6bV_VhQ4QjJS_Bxg">
                <div class="_3b0U-QDD-uhFpw6xM716fw">Steam Skin</div>
                    <div class="_2ZQ9wHACVFqZcufK_WRGPM">
                        <div class="_3N47t_-VlHS8JAEptE5rlR">
                            <button id="openMillennium" type="type" class="settings_SettingsDialogButton_3epr8 DialogButton _DialogLayout Secondary Focusable" tabindex="-1">Open Millennium</button>
                        </div>
                    </div>
                </div>
                <div class="_2OJfkxlD3X9p8Ygu1vR7Lr">
                <div>Select the skin you wish Steam to use (requires reload) </div>
            </div>
        </div>
    )";
}

const std::string get_drop_down_description(std::string name, std::string current, std::string desc, std::vector<std::string> options) {

    static int number = 0;
    number += 1;

    std::string options_html;

    for (const auto item : options)
    {
        options_html += fmt::format("<div class=\"_1R-DVEa2yqX0no8BYLtn9N\">{}</div>", item);
    }

    return R"(
		<div class="S-_LaQG5eEOM2HWZ-geJI qFXi6I-Cs0mJjTjqGXWZA _3XNvAmJ9bv_xuKx5YUkP-5 _3bMISJvxiSHPx1ol-0Aswn _3s1Rkl6cFOze_SdV2g-AFo _1ugIUbowxDg0qM0pJUbBRM _5UO-_VhgFhDWlkDIOZcn_ XRBFu6jAfd5kH9a3V8q_x wE4V6Ei2Sy2qWDo_XNcwn Panel" tabindex="-1" style="--indent-level:0;">
			<div class="H9WOq6bV_VhQ4QjJS_Bxg">
				<div class="_3b0U-QDD-uhFpw6xM716fw">)"+ name +R"(</div>
				<div class="_2ZQ9wHACVFqZcufK_WRGPM">
					<div class="_3N47t_-VlHS8JAEptE5rlR">
						<div class="DialogDropDown _DialogInputContainer  Panel" tabindex="-1" id="drop_down_)"+ std::to_string(number) +R"(" onclick="drop_down_f_)"+ std::to_string(number) +R"(() ">
							<div class="DialogDropDown_CurrentDisplay" id="drop_down_current)"+ std::to_string(number) +R"(">)" + current + R"(</div>
							<div class="DialogDropDown_Arrow">
								<svg xmlns="http://www.w3.org/2000/svg" class="SVGIcon_Button SVGIcon_DownArrowContextMenu" data-name="Layer 1" viewBox="0 0 128 128" x="0px" y="0px">
									<polygon points="50 59.49 13.21 22.89 4.74 31.39 50 76.41 95.26 31.39 86.79 22.89 50 59.49"></polygon>
								</svg>
							</div>
						</div>
					</div>
				</div>
			</div>
			<div class="_2OJfkxlD3X9p8Ygu1vR7Lr">)"+ desc +R"(</div>
		</div>

        <script>
            async function drop_down_f_)"+ std::to_string(number) +R"(() {
                let el = document.getElementById('drop_down_)"+ std::to_string(number) +R"(')
                console.log('clicked a dropdown', el)
                const rect = el.getBoundingClientRect();

                await new Promise(resolve => setTimeout(resolve, 100));

                document.body.insertAdjacentHTML('afterbegin', \`<div class="dialog_overlay"></div>
                    <div class="DialogMenuPosition visible _2qyBZV8YvxstXuSKiYDF19" tabindex="0" style="position: absolute; visibility: visible; height: fit-content;">
                      <div class="_1tiuYeMmTc9DMG4mhgaQ5w _DialogInputContainer" id="millennium-dropdown">
                        )" + options_html + R"(
                      </div>
                    </div>
                \`);

                const dialog = document.querySelector("._2qyBZV8YvxstXuSKiYDF19")
                dialog.style.top = rect.bottom + "px"
                dialog.style.left = rect.right - dialog.clientWidth + "px"
                dialog.focus()

                document.querySelectorAll('._1R-DVEa2yqX0no8BYLtn9N').forEach(item => {
                  item.addEventListener('click', function(event) {
                    console.log(')" + name + R"( set to ', event.target.innerText)
                    document.querySelector('#drop_down_current)"+ std::to_string(number) +R"(').innerText = event.target.innerText
                  });
                });
            }
        </script>
    )";
}

const std::string get_toggle_description(std::string name, std::string description, bool enabled) {

    static int number = 0;
    number += 1;

	return R"(
		<div class="S-_LaQG5eEOM2HWZ-geJI qFXi6I-Cs0mJjTjqGXWZA _3XNvAmJ9bv_xuKx5YUkP-5 _3bMISJvxiSHPx1ol-0Aswn _3s1Rkl6cFOze_SdV2g-AFo _1ugIUbowxDg0qM0pJUbBRM _5UO-_VhgFhDWlkDIOZcn_ XRBFu6jAfd5kH9a3V8q_x wE4V6Ei2Sy2qWDo_XNcwn Panel" tabindex="-1" style="--indent-level:0;">
			<div class="H9WOq6bV_VhQ4QjJS_Bxg">
				<div class="_3b0U-QDD-uhFpw6xM716fw">)" + name + R"(</div>
				<div class="_2ZQ9wHACVFqZcufK_WRGPM">
					<div class="_3N47t_-VlHS8JAEptE5rlR">
						<div class="_24G4gV0rYtRbebXM44GkKk )" + (enabled ? "_3ld7THBuSMiFtcB_Wo165i" : "") + R"( Focusable" id='toggle_desc_)" + std::to_string(number) + R"(' tabindex="-1" onclick='toggle_desc_)" + std::to_string(number) + R"(()'>
							<div class="_2JtC3JSLKaOtdpAVEACsG1"></div>
							<div class="_3__ODLQXuoDAX41pQbgHf9"></div>
						</div>
					</div>
				</div>
			</div>
			<div class="_2OJfkxlD3X9p8Ygu1vR7Lr">)" + description + R"(</div>
		</div>

        <script>
            function toggle_desc_)" + std::to_string(number) + R"(() {
                const result = document.getElementById('toggle_desc_)" + std::to_string(number) + R"(').classList.toggle('_3ld7THBuSMiFtcB_Wo165i')
                console.log('updated a checkbox:', result)
            }
        </script>
	)";
}

const std::string get_toggle() {
    return R"(
        <div class="S-_LaQG5eEOM2HWZ-geJI qFXi6I-Cs0mJjTjqGXWZA _3XNvAmJ9bv_xuKx5YUkP-5 _3s1Rkl6cFOze_SdV2g-AFo _5UO-_VhgFhDWlkDIOZcn_ XRBFu6jAfd5kH9a3V8q_x wE4V6Ei2Sy2qWDo_XNcwn Panel" tabindex="-1" style="--indent-level:0;">
            <div class="H9WOq6bV_VhQ4QjJS_Bxg">
                <div class="_3b0U-QDD-uhFpw6xM716fw">Scale text and icons to match monitor settings (requires restart)</div>
                    <div class="_2ZQ9wHACVFqZcufK_WRGPM">
                        <div class="_3N47t_-VlHS8JAEptE5rlR">
                            <div class="_24G4gV0rYtRbebXM44GkKk  Focusable" tabindex="-1">
                                <div class="_2JtC3JSLKaOtdpAVEACsG1"></div>
                            <div class="_3__ODLQXuoDAX41pQbgHf9"></div>
                        </div>
                    </div>
                </div>
            </div>
        </div>
    )";
}

const std::string get(std::string title, std::vector<std::string> options) {

    std::string concat;

    for (const auto& option : options) {
        concat += option + '\n';
    }

	return R"(
console.log("creating a window")
function CreateWindow(browserViewOptions, windowOptions) {
    let wnd = window.open(
        'about:blank?createflags=274&minwidth=850&minheight=600',
        undefined, Object.entries(windowOptions).map(e => e.join('=')).join(','), false
    );

    wnd.document.write(`
<html class="client_chat_frame fullheight ModalDialogPopup millennium_editor" lang="en">
  <head>
    <link href="/css/library.css" rel="stylesheet">
    <link rel="stylesheet" type="text/css" href="/css/libraries/libraries~2dcc5aaf7.css">
    <link rel="stylesheet" type="text/css" href="/css/chunk~2dcc5aaf7.css">
    <title>Millennium &#x2022 )" + title + R"(</title>
  </head>
  <body class="fullheight ModalDialogBody DesktopUI">
    <div id="popup_target" class="fullheight">
      <div class="PopupFullWindow">
        <div class="TitleBar title-area">
          <div class="title-area-highlight"></div>
          <div class="title-area-children"></div>
          <div class="title-bar-actions window-controls">
            <div class="title-area-icon closeButton windowControlButton">
              <div class="title-area-icon-inner">
                <svg version="1.1" id="Layer_2" xmlns="http://www.w3.org/2000/svg" class="SVGIcon_Button SVGIcon_X_Line" x="0px" y="0px" width="256px" height="256px" viewBox="0 0 256 256">
                  <line fill="none" stroke="#FFFFFF" stroke-width="45" stroke-miterlimit="10" x1="212" y1="212" x2="44" y2="44"></line>
                  <line fill="none" stroke="#FFFFFF" stroke-width="45" stroke-miterlimit="10" x1="44" y1="212" x2="212" y2="44"></line>
                </svg>
              </div>
            </div>
            <div class="title-area-icon maximizeButton windowControlButton">
              <div class="title-area-icon-inner">
                <svg version="1.1" id="base" xmlns="http://www.w3.org/2000/svg" class="SVGIcon_Button SVGIcon_Maximize" x="0px" y="0px" width="256px" height="256px" viewBox="0 0 256 256">
                  <rect x="24" y="42.167" fill="none" stroke='rgb(120, 138, 146)' stroke-width="18" stroke-miterlimit="10" width="208" height="171.667"></rect>
                  <line fill="none" stroke='rgb(120, 138, 146)' stroke-width="42" stroke-miterlimit="10" x1="24" y1="54.01" x2="232" y2="54.01"></line>
                </svg>
              </div>
            </div>
            <div class="title-area-icon minimizeButton windowControlButton">
              <div class="title-area-icon-inner">
                <svg version="1.1" id="base" xmlns="http://www.w3.org/2000/svg" class="SVGIcon_Button SVGIcon_Minimize" x="0px" y="0px" width="256px" height="256px" viewBox="0 0 256 256">
                  <line fill="none" stroke='rgb(120, 138, 146)' stroke-width="18" stroke-miterlimit="10" x1="24" y1="209.01" x2="232" y2="209.01"></line>
                </svg>
              </div>
            </div>
          </div>
        </div>
        <div class="FullModalOverlay" style="display: none;">
          <div class="ModalOverlayContent ModalOverlayBackground"></div>
        </div>
        <div class="ModalPosition" tabindex="0">
          <div class="ModalPosition_Content">
            <div class="ModalPosition_TopBar"></div>
            <div class="ModalPosition_Dismiss">
              <div class="closeButton">
                <svg version="1.1" id="Layer_2" xmlns="http://www.w3.org/2000/svg" class="SVGIcon_Button SVGIcon_X_Line" x="0px" y="0px" width="256px" height="256px" viewBox="0 0 256 256">
                  <line fill="none" stroke="#FFFFFF" stroke-width="45" stroke-miterlimit="10" x1="212" y1="212" x2="44" y2="44"></line>
                  <line fill="none" stroke="#FFFFFF" stroke-width="45" stroke-miterlimit="10" x1="44" y1="212" x2="212" y2="44"></line>
                </svg>
              </div>
            </div>
            <div class="_3I6h_oySuLmmLY9TjIKT9s _3few7361SOf4k_YuKCmM62 MzjwfxXSiSauz8kyEuhYO Panel" tabindex="-1">
              <div class="DialogContentTransition Panel" tabindex="-1" style="width: 100vw !important; min-width: 100vw !important;">
                <div class="DialogContent _DialogLayout _1I3NifxqTHCkE-2DeritAs " style="padding: 24px !important;">
                  <div class="DialogContent_InnerWidth">
                    <div class="DialogHeader">Editing )" + title + R"(</div>
                        )" + concat + R"(
                    </div>
                  </div>
                </div>
              </div>
            </div>
          </div>
        </div>
      </div>
      <div class="window_resize_grip"></div>
    </div>
  </body>

  <script>
    document.querySelector(".maximizeButton").addEventListener('click', () => {
        SteamClient.Window.ToggleMaximize()
    })
    document.querySelector(".closeButton").addEventListener('click', () => {
        SteamClient.Window.Close()
    })
    document.querySelector(".minimizeButton").addEventListener('click', () => {
        SteamClient.Window.Minimize()
    })
  </script>

<style>
.DialogContent_InnerWidth {
    margin-top: 25px !important;
}
</style>
    
</html>`)
    wnd.SteamClient.Window.ShowWindow()
	wnd.SteamClient.Window.SetResizeGrip(8, 8)
}

let width = 650
let height = 400

let wnd = CreateWindow({}, {
    width: width,
    height: height,
    top: (screen.width / 2) - (width / 2),
    left: (screen.height / 2) - (height / 2),
    createflags: 274,
    resizable: false,
    status: false,
    toolbar: true,
    menubar: false,
    location: true,
});
)";
}

std::string editor::create()
{
    std::cout << "Calling edit function" << std::endl;
    const auto themeData = skin_json_config;

    std::vector<std::string> output;

    enum type
    {
        checkbox,
        combobox
    };

    if (!skin_json_config.contains("Conditions"))
    {
        return "console.err('nothing to edit')";
    }

    nlohmann::basic_json<> g_conditionals = conditionals::get_conditionals(skin_json_config["native-name"]);

    for (auto& [key, condition] : skin_json_config["Conditions"].items()) {

        if (!condition.contains("values")) {
            continue;
        }

        const std::string description = condition.value("description", "Nothing here...");

        const auto get_type = ([&](void) -> type
            {
                bool is_bool = condition["values"].contains("yes") && condition["values"].contains("no");
                return is_bool ? checkbox : combobox;
            });

        const std::string theme_name = skin_json_config["native-name"];

        switch (get_type())
        {
        case checkbox:
        {
            const std::string str_val = g_conditionals[key];

            bool value = str_val == "yes" ? true : false;

            output.push_back(get_toggle_description(key.c_str(), description, value));

            break;
        }
        case combobox:
        {
            const std::string value = g_conditionals[key];
            const int combo_size = condition["values"].size();

            std::vector<ComboBoxItem> comboBoxItems;

            if (!comboAlreadyExists(key))
            {
                int out = -1;
                int i = 0;
                for (auto& [_val, options] : condition["values"].items())
                {
                    if (_val == value) { out = i; }
                    i++;
                }
                comboBoxItems.push_back({ key, out });
            }

            int out = 0;

            for (const auto& item : comboBoxItems)
            {
                if (item.name == key)
                    out = item.value;
            }

            std::vector<std::string> items;

            for (auto& el : condition["values"].items())
            {
                items.push_back(el.key());
            }

            output.push_back(get_drop_down_description(key.c_str(), items[out], description, items));

            break;
        }
        }

    }

//    steam_js_context context;
//    context.exec_command(get(themeData.value("name", "null"), output));
//

    return get(themeData.value("name", "null"), output);
}
