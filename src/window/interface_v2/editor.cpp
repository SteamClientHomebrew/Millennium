#include <stdafx.h>
#include "editor.hpp"
#include <utils/config/config.hpp>
#include <core/injector/event_handler.hpp>
#include <core/steam/cef_manager.hpp>

const std::string get_button_description(std::string title, std::string description, std::string buttonText) {
    return R"(
        <div class="gamepaddialog_Field_S-_La gamepaddialog_WithFirstRow_qFXi6 gamepaddialog_VerticalAlignCenter_3XNvA gamepaddialog_WithDescription_3bMIS gamepaddialog_WithBottomSeparatorStandard_3s1Rk gamepaddialog_ChildrenWidthFixed_1ugIU gamepaddialog_ExtraPaddingOnChildrenBelow_5UO-_ gamepaddialog_StandardPadding_XRBFu gamepaddialog_HighlightOnFocus_wE4V6 Panel" tabindex="-1" style="--indent-level:0">
            <div class="gamepaddialog_FieldLabelRow_H9WOq">
                <div class="gamepaddialog_FieldLabel_3b0U-">Steam Skin</div>
                    <div class="gamepaddialog_FieldChildrenWithIcon_2ZQ9w">
                        <div class="gamepaddialog_FieldChildrenInner_3N47t">
                            <button id="openMillennium" type="type" class="settings_SettingsDialogButton_3epr8 DialogButton _DialogLayout Secondary Focusable" tabindex="-1">Open Millennium</button>
                        </div>
                    </div>
                </div>
                <div class="gamepaddialog_FieldDescription_2OJfk">
                <div>Select the skin you wish Steam to use (requires reload) </div>
            </div>
        </div>
    )";
}

const std::string get_drop_down_description() {
    return R"(
		<div class="gamepaddialog_Field_S-_La gamepaddialog_WithFirstRow_qFXi6 gamepaddialog_VerticalAlignCenter_3XNvA gamepaddialog_WithDescription_3bMIS gamepaddialog_WithBottomSeparatorStandard_3s1Rk gamepaddialog_ChildrenWidthFixed_1ugIU gamepaddialog_ExtraPaddingOnChildrenBelow_5UO-_ gamepaddialog_StandardPadding_XRBFu gamepaddialog_HighlightOnFocus_wE4V6 Panel" tabindex="-1" style="--indent-level:0;">
			<div class="gamepaddialog_FieldLabelRow_H9WOq">
				<div class="gamepaddialog_FieldLabel_3b0U-">Steam Client Language</div>
				<div class="gamepaddialog_FieldChildrenWithIcon_2ZQ9w">
					<div class="gamepaddialog_FieldChildrenInner_3N47t">
						<div class="DialogDropDown _DialogInputContainer  Panel" tabindex="-1">
							<div class="DialogDropDown_CurrentDisplay">English</div>
							<div class="DialogDropDown_Arrow">
								<svg xmlns="http://www.w3.org/2000/svg" class="SVGIcon_Button SVGIcon_DownArrowContextMenu" data-name="Layer 1" viewBox="0 0 128 128" x="0px" y="0px">
									<polygon points="50 59.49 13.21 22.89 4.74 31.39 50 76.41 95.26 31.39 86.79 22.89 50 59.49"></polygon>
								</svg>
							</div>
						</div>
					</div>
				</div>
			</div>
			<div class="gamepaddialog_FieldDescription_2OJfk">Select the language you want Steam to use (requires restart)</div>
		</div>    
    )";
}

const std::string get_toggle_description(std::string name, std::string description, bool enabled) {

    static int number = 0;
    number += 1;

	return R"(
		<div class="gamepaddialog_Field_S-_La gamepaddialog_WithFirstRow_qFXi6 gamepaddialog_VerticalAlignCenter_3XNvA gamepaddialog_WithDescription_3bMIS gamepaddialog_WithBottomSeparatorStandard_3s1Rk gamepaddialog_ChildrenWidthFixed_1ugIU gamepaddialog_ExtraPaddingOnChildrenBelow_5UO-_ gamepaddialog_StandardPadding_XRBFu gamepaddialog_HighlightOnFocus_wE4V6 Panel" tabindex="-1" style="--indent-level:0;">
			<div class="gamepaddialog_FieldLabelRow_H9WOq">
				<div class="gamepaddialog_FieldLabel_3b0U-">)" + name + R"(</div>
				<div class="gamepaddialog_FieldChildrenWithIcon_2ZQ9w">
					<div class="gamepaddialog_FieldChildrenInner_3N47t">
						<div class="gamepaddialog_Toggle_24G4g )" + (enabled ? "gamepaddialog_On_3ld7T" : "") + R"( Focusable" id='toggle_desc_)" + std::to_string(number) + R"(' tabindex="-1" onclick='toggle_desc_)" + std::to_string(number) + R"(()'>
							<div class="gamepaddialog_ToggleRail_2JtC3"></div>
							<div class="gamepaddialog_ToggleSwitch_3__OD"></div>
						</div>
					</div>
				</div>
			</div>
			<div class="gamepaddialog_FieldDescription_2OJfk">)" + description + R"(</div>
		</div>

        <script>
            function toggle_desc_)" + std::to_string(number) + R"(() {
                const result = document.getElementById('toggle_desc_)" + std::to_string(number) + R"(').classList.toggle('gamepaddialog_On_3ld7T')
                console.log('updated a checkbox:', result)
            }
        </script>
	)";
}

const std::string get_toggle() {
    return R"(
        <div class="gamepaddialog_Field_S-_La gamepaddialog_WithFirstRow_qFXi6 gamepaddialog_VerticalAlignCenter_3XNvA gamepaddialog_WithBottomSeparatorStandard_3s1Rk gamepaddialog_ExtraPaddingOnChildrenBelow_5UO-_ gamepaddialog_StandardPadding_XRBFu gamepaddialog_HighlightOnFocus_wE4V6 Panel" tabindex="-1" style="--indent-level:0;">
            <div class="gamepaddialog_FieldLabelRow_H9WOq">
                <div class="gamepaddialog_FieldLabel_3b0U-">Scale text and icons to match monitor settings (requires restart)</div>
                    <div class="gamepaddialog_FieldChildrenWithIcon_2ZQ9w">
                        <div class="gamepaddialog_FieldChildrenInner_3N47t">
                            <div class="gamepaddialog_Toggle_24G4g  Focusable" tabindex="-1">
                                <div class="gamepaddialog_ToggleRail_2JtC3"></div>
                            <div class="gamepaddialog_ToggleSwitch_3__OD"></div>
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
function CreateWindow(strURL, browserViewOptions, windowOptions) {

    let wnd = window.open(
        'about:blank?createflags=274&minwidth=850&minheight=600',
        undefined,
		Object.entries(windowOptions).map(e => e.join('=')).join(','),
        false
    );

    wnd.location = strURL;
    wnd.document.write(`
<html class="client_chat_frame fullheight ModalDialogPopup millennium_editor" lang="en">
  <head>
    <link href="/css/library.css" rel="stylesheet">
    <link rel="stylesheet" type="text/css" href="/css/libraries/libraries~2dcc5aaf7.css">
    <link rel="stylesheet" type="text/css" href="/css/chunk~2dcc5aaf7.css">
    <title>Millennium &#x2022 Editing )" + title + R"(</title>
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
            <div class="pagedsettings_PagedSettingsDialog_3I6h_ settings_SettingsModal_3few7 settings_DesktopPopup_Mzjwf Panel" tabindex="-1">
              <div class="DialogContentTransition Panel" tabindex="-1" style="width: 100vw !important; min-width: 100vw !important;">
                <div class="DialogContent _DialogLayout pagedsettings_PagedSettingsDialog_PageContent_1I3Ni " style="padding: 24px !important;">
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
    
</html>`)

    wnd.SteamClient.Window.ShowWindow()
	wnd.SteamClient.Window.SetResizeGrip(8, 8)
}

var width = 850
var height = 600

var x = (screen.width / 2) - (700 / 2);
var y = (screen.height / 2) - (450 / 2);

let wnd = CreateWindow('https://google.com', {}, {
    width: width,
    height: height,
    top: x,
    left: y,
    createflags: 274,
    resizable: false,
    status: false,
    toolbar: true,
    menubar: false,
    location: true,
});
)";
}

void editor::create()
{
    const auto themeData = skin_json_config;

    const bool HAS_CONFIG = themeData.contains("Configuration");
    const bool HAS_COLORS = themeData.contains("GlobalsColors");

    if (!HAS_CONFIG && !HAS_COLORS)
        return;

    std::vector<std::string> output;

    if (HAS_CONFIG) {
        for (const auto& config : themeData["Configuration"]) {
            auto type = config.value("Type", "__null__");

            if (type == "CheckBox") {

                auto name = config.value("Name", "empty");
                auto description = config.value("ToolTip", "empty");
                auto value = config.value("Value", false);

                output.push_back(get_toggle_description(name, description, value));
            }
        }
    }
    if (HAS_COLORS) {
        for (const auto& color : themeData["GlobalsColors"]) {

        }
    }

    steam_js_context context;
    context.exec_command(get(themeData.value("name", "null"), output));
}
