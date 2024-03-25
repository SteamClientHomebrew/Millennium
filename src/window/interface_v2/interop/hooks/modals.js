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

const modal = {
    remove_all: () => {
      document.querySelectorAll("._2qyBZV8YvxstXuSKiYDF19").forEach(el => el.remove())
    },
    set_position: (rect) => {
      function is_outside(pos) {
        return (
            pos.y < 0 || pos.y > window.innerHeight
        );
      }
      const dialog = document.querySelector("._2qyBZV8YvxstXuSKiYDF19")
      const set_top = () => {
        dialog.style.top = rect.top - dialog.clientHeight + "px"
        dialog.style.left = rect.right - dialog.clientWidth + "px"
        dialog.focus()
      }
      const set_bottom = () => {
        dialog.style.top = rect.bottom + "px"
        dialog.style.left = rect.right - dialog.clientWidth + "px"
        dialog.focus()
      }

      is_outside({ y: rect.top + dialog.clientHeight + 50 }) ?  set_top() : set_bottom()
    }
}

export { modal }