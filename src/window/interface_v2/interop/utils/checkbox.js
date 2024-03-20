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

export { create_checkbox }
