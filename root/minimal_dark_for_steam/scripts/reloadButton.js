/*jshint esversion: 8 */

import { waitForElement } from './waitForElement.js';

async function reloadButton()
{
  if(document.title !== "Steam Root Menu") return;
  
  try {
    const { querySelector, matchedElements } = await waitForElement('[class^="menu_MenuItem_"]', 3000);
    console.log('awaited code', querySelector, matchedElements);
    let targetWindow = window.opener;
    // do things with the matchedElements
    matchedElements.forEach((element) => {
      if (element.innerHTML.trim() === targetWindow.LocalizationManager.LocalizeString("#Menu_Exit")) {
        const clonedElement = element.cloneNode(true);
        clonedElement.innerHTML = targetWindow.LocalizationManager.LocalizeString("#BrowserContextMenu_Reload");
        clonedElement.onclick = function() {
          console.log("Reload clicked");
          targetWindow.eval("location.reload()");
        };
        element.parentNode.insertBefore(clonedElement, element);
        return false;
      }
    });
  } catch(error) {
    console.error(error, 'took to long to find element');
  }
}

reloadButton();