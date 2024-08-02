import { Millennium, pluginSelf } from "@millennium/ui";
import { ConditionalControlFlowType as ModuleType, Patch, ThemeItem } from "../types";
import { DOMModifier, classListMatch, constructThemePath } from "./Dispatch";
import { EvaluateConditions } from "./v2/Conditions";
import { PatchV1, EvaluateStatements } from "./v1/Conditions"

const EvaluateModule = (module: string, type: ModuleType, document: Document) => {

    const activeTheme: ThemeItem = pluginSelf.activeTheme

    switch (type) {
        case ModuleType.TargetCss:
            DOMModifier.AddStyleSheet(document, constructThemePath(activeTheme.native, module))
            break
        case ModuleType.TargetJs:
            DOMModifier.AddJavaScript(document, constructThemePath(activeTheme.native, module))
            break
    }
}

/**
 * @brief evaluates list of; or single module
 * 
 * @param module module(s) to be injected into the frame
 * @param type the type of the module
 * @returns null
 */
const SanitizeTargetModule = (module: string | Array<string>, type: ModuleType, document: Document) => {

    if (module === undefined) {
        return
    }
    else if (typeof module === 'string') {
        EvaluateModule(module, type, document)
    }
    else if (Array.isArray(module)) {
        module.forEach((node) => EvaluateModule(node, type, document));
    }
}

const EvaluatePatches = (activeTheme: ThemeItem, documentTitle: string, classList: string[], document: Document, context: any) => {
    
    activeTheme.data.Patches.forEach((patch: Patch) => {

        const match = patch.MatchRegexString
        context.m_popup.window.HAS_INJECTED_THEME = true

        if (!documentTitle.match(match) && !classListMatch(classList, match)) {
            return 
        }

        SanitizeTargetModule(patch?.TargetCss, ModuleType.TargetCss, document)
        SanitizeTargetModule(patch?.TargetJs, ModuleType.TargetJs, document)

        // backwards compatability with old millennium versions. 
        const PatchV1 = (patch as PatchV1)

        if (pluginSelf.conditionVersion == 1 && PatchV1?.Statement !== undefined) {
            EvaluateStatements(PatchV1, document)
        }
    });
}

/**
 * parses all classnames from a window and concatenates into one list
 * @param context window context from g_popupManager
 * @returns 
 */
const getDocumentClassList = (context: any): string[] => {

    const bodyClass: string = context?.m_rgParams?.body_class ?? String()
    const htmlClass: string = context?.m_rgParams?.html_class ?? String()

    return (`${bodyClass} ${htmlClass}`).split(' ').map((className: string) => '.' + className)
}

function patchDocumentContext(windowContext: any) 
{
    if (pluginSelf.isDefaultTheme) {
        return
    }

    const activeTheme: ThemeItem = pluginSelf.activeTheme
    const document: Document     = windowContext.m_popup.document    
    const classList: string[]    = getDocumentClassList(windowContext);
    const documentTitle: string  = windowContext.m_strTitle

    // Append System Accent Colors to global document (publically shared)
    DOMModifier.AddStyleSheetFromText(document, pluginSelf.systemColor, "SystemAccentColorInject")
    // Append old global colors struct to DOM
    pluginSelf?.GlobalsColors && DOMModifier.AddStyleSheetFromText(document, pluginSelf.GlobalsColors, "GlobalColors")

    if (activeTheme?.data?.Conditions) {
        pluginSelf.conditionVersion = 2
        EvaluateConditions(activeTheme, documentTitle, classList, document)
    }
    else {
        pluginSelf.conditionVersion = 1
    }
    activeTheme?.data?.hasOwnProperty("Patches") && EvaluatePatches(activeTheme, documentTitle, classList, document, windowContext)
    if (activeTheme?.data?.hasOwnProperty("RootColors")) {
        Millennium.callServerMethod("cfg.get_colors").then((colors: any) => {      
            DOMModifier.AddStyleSheetFromText(document, colors, "RootColors")
        })
    }
}

export { patchDocumentContext }