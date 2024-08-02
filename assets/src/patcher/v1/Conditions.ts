/**
 * @deprecated this entire module is deprecated and is only here to support Millennium <= 1.1.5
 * 
 * @note this module does not provide interfaces to edit the deprecated conditions, 
 * it serves only to allow old ones to still work until they are properly updated by the developer. 
 */

import { pluginSelf } from "@millennium/ui"
import { Patch, Theme, ThemeItem } from "../../types"
import { DOMModifier, constructThemePath } from "../Dispatch"

export interface EvaluateTargetProps {
    TargetCss: string,
    TargetJs: string
}

export interface V1StatementComboItem {
    Equals: any,
    True?: EvaluateTargetProps,
    False?: EvaluateTargetProps
}

export interface V1StatementCombo {
    Combo: V1StatementComboItem[],
    If: any
}

export interface V1Statement {
    If: any,
    Equals: any,
    True?: EvaluateTargetProps,
    False?: EvaluateTargetProps
}

export interface PatchV1 extends Patch {
    Statement: V1Statement | V1StatementCombo | V1StatementCombo[] | V1Statement[]
}

enum ConfigurationItemType {
    ComboBox = "ComboBox",
    CheckBox = "CheckBox"
}

export interface ConfigurationItem {
    Items?: string[],
    Name: string,
    ToolTip?: string,
    Type: ConfigurationItemType,
    Value: boolean | string
}

export interface ThemeItemV1 extends Theme {
    Configuration: ConfigurationItem[]
}

const GetFromConfigurationStore = (configName: string): ConfigurationItem | undefined => {
    
    const activeTheme: ThemeItemV1 = pluginSelf.activeTheme.data
    
    for (const configItem of activeTheme.Configuration) {
        if (configItem.Name === configName) {
            return configItem;
        }
    }

    return undefined
}

const InsertModule = (target: EvaluateTargetProps, document: Document) => {
    
    const activeTheme: ThemeItem = pluginSelf.activeTheme

    target?.TargetCss && DOMModifier.AddStyleSheet(document, constructThemePath(activeTheme.native, target.TargetCss))
    target?.TargetJs  && DOMModifier.AddJavaScript(document, constructThemePath(activeTheme.native, target.TargetJs))
}

const EvaluateComboBox = (statement: V1StatementCombo, currentValue: any, document: Document) => {

    statement.Combo.forEach((comboItem) => {

        if (comboItem.Equals === currentValue) {
            InsertModule(comboItem?.True, document)
        }
        else {
            InsertModule(comboItem?.False, document)
        }
    })
}

const EvaluateCheckBox = (statement: V1Statement, currentValue: any, document: Document) => {

    if (statement.Equals === currentValue) {
        InsertModule(statement?.True, document)
    }
    else {
        InsertModule(statement?.False, document)
    }
}

const EvaluateType = (statement: V1StatementCombo) => {
    return statement.Combo !== undefined ? ConfigurationItemType.ComboBox : ConfigurationItemType.CheckBox
}

const EvaluateStatement = (statement: V1Statement | V1StatementCombo, document: Document) => {
    
    const statementId = statement.If
    const statementStore = GetFromConfigurationStore(statementId)
    const storedStatementValue = statementStore.Value
    const statementType = EvaluateType(statement as V1StatementCombo)

    switch (statementType) {
        case ConfigurationItemType.CheckBox: {
            EvaluateCheckBox(statement as V1Statement, storedStatementValue, document)
            break
        }
        case ConfigurationItemType.ComboBox: {
            EvaluateComboBox(statement as V1StatementCombo, storedStatementValue, document)
            break
        }
    }
} 

export const EvaluateStatements = (patchItem: PatchV1, document: Document) => {

    if (Array.isArray(patchItem.Statement)) {
        patchItem.Statement.forEach(statement => {
            EvaluateStatement(statement, document)
        })
    }
    else {
        EvaluateStatement(patchItem.Statement, document)
    }
}