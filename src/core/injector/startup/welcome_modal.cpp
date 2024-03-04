#include "./welcome_modal.hpp"

std::string popupModal::getSnippet(std::string header, std::string body) {
    return R"(

        var millenniumClosedPopup = false;

        function disableBrowser() {
            window.opener.MainWindowBrowserManager.m_browser.SetVisible(false)

            if (millenniumClosedPopup) 
            {
                console.log("stopping set visibility")
                window.opener.MainWindowBrowserManager.m_browser.SetVisible(true)
                return
            }

            setTimeout(() => { disableBrowser() }, 100)
        } 

            (async () => {

                if (document.querySelector('.ModalOverlayContent.active') != null)
                    return

                disableBrowser()

                const modalOverlayContent = `
                <div class="ModalOverlayContent active">
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
                            <div class="DialogContent _DialogLayout GenericConfirmDialog changeuserdialog_Dialog_4076W _DialogCenterVertically">
                                <div class="DialogContent_InnerWidth">
                                    <form>
                                        <div class="DialogHeader">)" + header + R"(</div>
                                        <div class="DialogBody Panel">
                                            <div class="DialogBodyText">)" + body + R"(</div>
                                            <div class="DialogFooter">
                                                <div class="DialogTwoColLayout _DialogColLayout Panel">             
                                                    <button type="button" onclick='window.open("https://millennium.web.app/discord")' class="DialogButton _DialogLayout Secondary Focusable" tabindex="0">Join Discord</button>
                                                </div>
                                            </div>
                                        </div>
                                    </form>
                                </div>
                            </div>
                        </div>
                    </div>
                </div>
                `;
                const observer = new MutationObserver(() => {
                    const element = document.querySelector('.FullModalOverlay');
                    if (element != null) {
                        observer.disconnect();
                        element.insertAdjacentHTML('beforeend', modalOverlayContent)

                        document.querySelector('.ModalPosition_Dismiss .closeButton').addEventListener('click', () => {
                            millenniumClosedPopup = true;
                            element.style.display = "none"
                        })
                        element.style.display = "block"
                    }
                });
                observer.observe(document.body, { childList: true, subtree: true });
            })()
        )";
}