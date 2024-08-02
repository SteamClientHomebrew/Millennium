import { pluginSelf } from "@millennium/ui"
import { ColorProp } from "../../types"

/**
 * appends a virtual CSS script into self module
 * @param globalColors V1 Global Colors struct
 */
export const DispatchGlobalColors = (globalColors: ColorProp[]) => {
    pluginSelf.GlobalsColors = `
    :root {
        ${globalColors.map((color) => `${color.ColorName}: ${color.HexColorCode};`).join(" ")}
    }`
}