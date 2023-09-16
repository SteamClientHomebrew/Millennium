/*jshint esversion: 8 */

import { waitForElement } from './waitForElement.js';

async function restartButton()
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
        clonedElement.innerHTML = targetWindow.LocalizationManager.LocalizeString("#Restart");
        clonedElement.onclick = function() {
          console.log("Restart clicked");
          targetWindow.eval("SteamClient.User.StartRestart(false)");
        };
        element.parentNode.insertBefore(clonedElement, element);
        return false;
      }
    });
  } catch(error) {
    console.error(error, 'took to long to find element');
  }
}

restartButton();