import { pluginSelf } from '@steambrew/client';
import { Conditions, ConditionsStore, ThemeItem, ConditionalControlFlowType as ModuleType } from '../../types';
import { DOMModifier, EvaluatePatch } from '../Dispatch';

export function EvaluateConditions(theme: ThemeItem, title: string, classes: string[], document: Document): void {
	const themeConditions: Conditions = theme.data.Conditions ?? {};
	const savedConditions: ConditionsStore = pluginSelf.conditionals[theme.native];

	for (const condition in themeConditions) {
		if (!themeConditions.hasOwnProperty(condition)) {
			return;
		}

		const conditionDef = themeConditions[condition];

		if (conditionDef.slider && condition in savedConditions) {
			const { cssVariable, unit } = conditionDef.slider;
			const value = savedConditions[condition];
			DOMModifier.AddStyleSheetFromText(document, `:root { ${cssVariable}: ${value}${unit ?? ''}; }`, `millennium-slider-${cssVariable}`);
			continue;
		}

		if (condition in savedConditions && conditionDef.values) {
			const patch = conditionDef.values[savedConditions[condition]];

			EvaluatePatch(ModuleType.TargetCss, patch, title, classes, document);
			EvaluatePatch(ModuleType.TargetJs, patch, title, classes, document);
		}
	}
}
