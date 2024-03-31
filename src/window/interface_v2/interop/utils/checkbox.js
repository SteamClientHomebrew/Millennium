let id = 0

function create_checkbox(name, enabled, description)
{
  id = id + 1
  const current = id

  window[`toggle_check${current}`] = () => {
    const result = document.getElementById(`toggle_desc_${current}`).classList.toggle('_3ld7THBuSMiFtcB_Wo165i')
    console.log("millennium.user.message:", JSON.stringify({id: `[set-${name}-status]`, value: result}))
  }

  return `
    <div class="S-_LaQG5eEOM2HWZ-geJI gamepaddialog_Field_S-_La qFXi6I-Cs0mJjTjqGXWZA gamepaddialog_WithFirstRow_qFXi6 _3XNvAmJ9bv_xuKx5YUkP-5 gamepaddialog_VerticalAlignCenter_3XNvA _3bMISJvxiSHPx1ol-0Aswn gamepaddialog_WithDescription_3bMIS _3s1Rkl6cFOze_SdV2g-AFo gamepaddialog_WithBottomSeparatorStandard_3s1Rk _5UO-_VhgFhDWlkDIOZcn_ gamepaddialog_ExtraPaddingOnChildrenBelow_5UO-_ XRBFu6jAfd5kH9a3V8q_x gamepaddialog_StandardPadding_XRBFu wE4V6Ei2Sy2qWDo_XNcwn gamepaddialog_HighlightOnFocus_wE4V6 Panel" style="--indent-level:0;">
      <div class="H9WOq6bV_VhQ4QjJS_Bxg gamepaddialog_FieldLabelRow_H9WOq">
        <div class="_3b0U-QDD-uhFpw6xM716fw gamepaddialog_FieldLabel_3b0U-">${name}</div>
        <div class="_2ZQ9wHACVFqZcufK_WRGPM gamepaddialog_FieldChildrenWithIcon_2ZQ9w">
          <div class="_3N47t_-VlHS8JAEptE5rlR gamepaddialog_FieldChildrenInner_3N47t">
            <div class="_24G4gV0rYtRbebXM44GkKk gamepaddialog_Toggle_24G4g ${enabled ? "_3ld7THBuSMiFtcB_Wo165i gamepaddialog_On_3ld7T" : ""} Focusable" id='toggle_desc_${current}' tabindex="0" onclick='toggle_check${current}()'>
              <div class="_2JtC3JSLKaOtdpAVEACsG1 gamepaddialog_ToggleRail_2JtC3"></div>
              <div class="_3__ODLQXuoDAX41pQbgHf9 gamepaddialog_ToggleSwitch_3__OD"></div>
            </div>
          </div>
        </div>
      </div>
      <div class="_2OJfkxlD3X9p8Ygu1vR7Lr gamepaddialog_FieldDescription_2OJfk">${description}</div>
    </div>`
}

export { create_checkbox }
