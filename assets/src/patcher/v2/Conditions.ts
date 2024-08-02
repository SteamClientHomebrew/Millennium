import { pluginSelf } from "@millennium/ui"
import { Conditions, ConditionsStore, ThemeItem, ConditionalControlFlowType as ModuleType } from "../../types"
import { EvaluatePatch } from "../Dispatch"

export const EvaluateConditions = (theme: ThemeItem, title: string, classes: string[], document: Document): void => {

    const themeConditions: Conditions = theme.data.Conditions
    const savedConditions: ConditionsStore = pluginSelf.conditionals[theme.native]

    for (const condition in themeConditions) {

        if (!themeConditions.hasOwnProperty(condition)) {
            return 
        }

        if (condition in savedConditions) {
            const patch = themeConditions[condition].values[savedConditions[condition]]

            EvaluatePatch(ModuleType.TargetCss, patch, title, classes, document)
            EvaluatePatch(ModuleType.TargetJs, patch, title, classes, document)
        }
    }
}