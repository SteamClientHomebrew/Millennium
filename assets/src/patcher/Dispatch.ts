import { pluginSelf } from "@millennium/ui";
import { CommonPatchTypes, ConditionalControlFlow, ConditionalPatch, ConditionalControlFlowType as ModuleType } from "../types";

export const DOMModifier = {
    /**
     * Append a StyleSheet to DOM from raw text
     * @param document Target document to append StyleSheet to
     * @param innerStyle string encoded CSS
     * @param id HTMLElement id
     */
    AddStyleSheetFromText: (document: Document, innerStyle: string, id?: string) => {
        if (document.querySelectorAll(`style[id='${id}']`).length) return 
        
        document.head.appendChild(Object.assign(document.createElement('style'), { id: id })).innerText = innerStyle
    },
    /**
     * Append a StyleSheet to DOM from loopbackhost or absolute URI
     * @param document Target document to append StyleSheet to
     * @param localPath relative/absolute path to CSS module
     */
    AddStyleSheet: (document: Document, localPath: string) => {   
        if (!pluginSelf.stylesAllowed) return 
        if (document.querySelectorAll(`link[href='${localPath}']`).length) return

        document.head.appendChild(Object.assign(document.createElement('link'), { 
            href: localPath, 
            rel: 'stylesheet', id: 'millennium-injected' 
        }));
    },
    /**
     * Append a JavaScript module to DOM from loopbackhost or absolute URI
     * @param document Target document to append JavaScript to
     * @param localPath relative/absolute path to CSS module
     */
    AddJavaScript: (document: Document, localPath: string) => {
        if (!pluginSelf.scriptsAllowed) return 
        if (document.querySelectorAll(`script[src='${localPath}'][type='module']`).length) return 
        
        document.head.appendChild(Object.assign(document.createElement('script'), { 
            src: localPath, 
            type: 'module', id: 'millennium-injected' 
        }));
    }
}

export function constructThemePath(nativeName: string, relativePath: string) {
    return ['skins', nativeName, relativePath].join('/');
}

export const classListMatch = (classList: string[], affectee: string) => {
    for (const classItem in classList) {
        if (classList[classItem].includes(affectee)) {
            return true
        }
    }
    return false
}

export const EvaluatePatch = (type: ModuleType, modulePatch: ConditionalControlFlow, documentTitle: string, classList: string[], document: Document) => {

    if ((modulePatch as any)?.[CommonPatchTypes?.[type]] === undefined) {
        return 
    }

    ((modulePatch as any)[CommonPatchTypes[type]] as ConditionalPatch).affects.forEach((affectee: string) => {

        if (!documentTitle.match(affectee) && !classListMatch(classList, affectee)) {
            return 
        }

        switch (type) {
            case ModuleType.TargetCss: {
                DOMModifier.AddStyleSheet(document, constructThemePath(pluginSelf.activeTheme.native, (modulePatch as any)[CommonPatchTypes[type]].src))  
            }   
            case ModuleType.TargetJs: {
                DOMModifier.AddJavaScript(document, constructThemePath(pluginSelf.activeTheme.native, (modulePatch as any)[CommonPatchTypes[type]].src))
            }     
        }  
    });
}