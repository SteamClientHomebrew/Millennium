import ReactDOM from "react-dom";
import { ReactNode } from "react";
import { TitleBar } from "./TitleBar";
import { CreatePopupBase } from "./CreatePopupBase";

export interface RenderProps {
    _window: Window
}

export class CreatePopup extends CreatePopupBase {

    constructor(component: ReactNode, strPopupName: string, options: any) {
        super(strPopupName, options)
        this.component = component
    }

    Show() {
        super.Show()
        const RenderComponent: React.FC<RenderProps> = ({_window}) => {

            return (
                <>
                    <div 
                        className="PopupFullWindow"
                        onContextMenu={((_e) => {
                            // console.log('CONTEXT MENU OPEN')
                            // _e.preventDefault()

                            // this.contextMenuHandler.CreateContextMenuInstance(_e)
                        })}
                        >
                        <TitleBar
                            popup={_window}
                            hideMin={false}
                            hideMax={false}
                            hideActions={false}
                        />
                        <this.component/>
        
                    </div>
                </>
        
            )
        }

        console.log(super.root_element)
        ReactDOM.render(<RenderComponent _window={super.window}/>, super.root_element)
    }

    SetTitle() {
        console.log("[internal] setting title ->", this)

        if (this.m_popup && this.m_popup.document) {
            this.m_popup.document.title = "WINDOW"
        }
    }

    Render(_window: Window, _element: HTMLElement) { }

    OnClose() { }

    OnLoad()  {
        const element: HTMLElement = (this.m_popup.document as Document).querySelector(".DialogContent_InnerWidth")
        const height = element?.getBoundingClientRect()?.height

        this.m_popup.SteamClient?.Window?.ResizeTo(450, height + 48, true)
        this.m_popup.SteamClient.Window.ShowWindow()
    }
}