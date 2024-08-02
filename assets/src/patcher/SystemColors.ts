import { pluginSelf } from "@millennium/ui"
import { SystemAccentColor } from "../types"

/**
 * appends a virtual CSS script into self module
 * @param systemColors SystemAccentColors
 */
export const DispatchSystemColors = (systemColors: SystemAccentColor) => {
    pluginSelf.systemColor = `
    :root {
        --SystemAccentColor: ${systemColors.accent}; 
        --SystemAccentColor-RGB: ${systemColors.accentRgb}; 
        --SystemAccentColorLight1: ${systemColors.light1}; 
        --SystemAccentColorLight1-RGB: ${systemColors.light1Rgb}; 
        --SystemAccentColorLight2: ${systemColors.light2}; 
        --SystemAccentColorLight2-RGB: ${systemColors.light2Rgb}; 
        --SystemAccentColorLight3: ${systemColors.light3};
        --SystemAccentColorLight3-RGB: ${systemColors.light3Rgb};
        --SystemAccentColorDark1: ${systemColors.dark1};
        --SystemAccentColorDark1-RGB: ${systemColors.dark1Rgb};
        --SystemAccentColorDark2: ${systemColors.dark2};
        --SystemAccentColorDark2-RGB: ${systemColors.dark2Rgb};
         --SystemAccentColorDark3: ${systemColors.dark3};
         --SystemAccentColorDark3-RGB: ${systemColors.dark3Rgb};
    }`
}