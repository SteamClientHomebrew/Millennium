/**
 * ==================================================
 *   _____ _ _ _             _
 *  |     |_| | |___ ___ ___|_|_ _ _____
 *  | | | | | | | -_|   |   | | | |     |
 *  |_|_|_|_|_|_|___|_|_|_|_|_|___|_|_|_|
 *
 * ==================================================
 *
 * Copyright (c) 2025 Project Millennium
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

import React, { useMemo, useState } from 'react';
import { Dropdown, SingleDropdownOption, Toggle, Field, findClassModule, TextField, findModuleDetailsByExport } from '@steambrew/client';
import Markdown from 'markdown-to-jsx';

interface ComponentInterface {
	object: string;
	component: any;
	type: ComponentType;
	pluginName: string;
}

interface PluginEditorProps {
	plugin: any;
}

enum ComponentType {
	DropDown,
	CheckBox,
	NumberTextInput,
	StringTextInput,
	FloatTextInput,
	NumberSlider,
	FloatSlider,
}

const SliderComponent = findModuleDetailsByExport(
	(m) => m?.prototype?.hasOwnProperty('BShouldTriggerHapticOnSnap') && m?.prototype?.hasOwnProperty('ComputeNormalizedValueForMousePosition'),
)[1];
console.log('SliderComponent', SliderComponent);

const RenderComponentType: React.FC<ComponentInterface> = ({ pluginName, object, component, type }) => {
	const settings = window.MILLENNIUM_PLUGIN_SETTINGS_STORE?.[pluginName].settingsStore;
	const items = component?.options?.map((value: string, index: number) => ({ label: value, data: 'componentId' + index }));

	const [checked, setChecked] = useState<boolean>(component.value);
	const [textValue, setTextValue] = useState<string | number>(component.value);
	const [sliderValue, setSliderValue] = useState<number>(component.value);

	const OnDropDownChange = (option: SingleDropdownOption) => {
		settings[object] = option.label;
	};

	const OnCheckBoxChange = (checked: boolean) => {
		settings[object] = checked;
		setChecked(checked);
	};

	function OnTextInputChange<T>(type: T, event: React.ChangeEvent<HTMLInputElement>) {
		console.log('Text input changed', event.target.value);

		try {
			if (type === ComponentType.StringTextInput) {
				settings[object] = event.target.value;
			} else if (type === ComponentType.FloatTextInput) {
				settings[object] = parseFloat(event.target.value);
			} else if (type === ComponentType.NumberTextInput) {
				settings[object] = parseInt(event.target.value);
			}

			setTextValue(settings[object]);
		} catch (e) {
			console.error('Error parsing number', e);
		}
	}

	function OnSliderChange(newValue: number) {
		settings[object] = newValue;
	}

	const textFieldClass = 'DialogInput DialogInputPlaceholder DialogTextInputBase Focusable';

	switch (type) {
		case ComponentType.DropDown:
			return (
				<Dropdown
					// key={pluginName + object}
					contextMenuPositionOptions={{ bMatchWidth: false }}
					onChange={OnDropDownChange}
					rgOptions={items}
					selectedOption={1}
					strDefaultLabel={component.value}
				/>
			);

		case ComponentType.CheckBox:
			return <Toggle key={pluginName + object} value={checked} onChange={OnCheckBoxChange} />;

		case ComponentType.NumberTextInput:
		case ComponentType.FloatTextInput:
		case ComponentType.StringTextInput:
			return <TextField key={pluginName + object} value={textValue as string} onChange={(event) => OnTextInputChange(type, event)} className={textFieldClass} />;

		case ComponentType.NumberSlider:
		case ComponentType.FloatSlider:
			return (
				<>
					<div className="MillenniumPluginSettingsSliderValue">{sliderValue}</div>
					<SliderComponent
						min={component.range[0]}
						max={component.range[1]}
						step={component.step}
						value={sliderValue}
						onChange={(newValue: number) => {
							setSliderValue(newValue);
						}}
						onChangeComplete={(newValue: number) => {
							OnSliderChange(newValue);
						}}
					/>
				</>
			);

		default:
			return <></>;
	}
};

const SettingsModalClass = (findClassModule((m) => m.SettingsModal) as any).SettingsModal;

const popupStyles = `
/** Override the default width of the popup */
.DialogContent._DialogLayout.GenericConfirmDialog._DialogCenterVertically {
	width: 635px;
}
/** Hide the cancel button as it doesn't apply here */
button.DialogButton._DialogLayout.Secondary.Focusable {
	display: none;
}
.${SettingsModalClass} {
	min-width: unset;
}
.DialogSlider_Slider {
    width: 200px;
}
`;

const RenderComponents: React.FC<PluginEditorProps> = ({ plugin }) => {
	const settings = window.MILLENNIUM_PLUGIN_SETTINGS_STORE?.[plugin?.data?.name].settingsStore.__raw_get_internals__;

	const components = useMemo(
		() =>
			Object.keys(settings).map((name, index) => {
				const value = settings[name];

				return (
					<Field
						label={value.name}
						description={<Markdown>{value?.desc ?? 'No description yet.'}</Markdown>}
						key={index}
						bottomSeparator={Object.keys(settings).length - 1 === index ? 'none' : 'standard'}
						focusable
						inlineWrap={'shift-children-below'}
						verticalAlignment={'none'}
					>
						<RenderComponentType pluginName={plugin?.data?.name} object={name} component={value} type={ComponentType[value.type as keyof typeof ComponentType]} />
					</Field>
				);
			}),
		[],
	);

	return (
		<div className="MillenniumPluginSettingsGrid _1u5y7VbwvOwncjMsMqnFUz Panel">
			<style>{popupStyles}</style>
			{components}
		</div>
	);
};

export { RenderComponents };
