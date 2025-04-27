import React, { useState } from 'react';
import { Dropdown, SingleDropdownOption, Toggle, Field, findClassModule, TextField, SliderField, findModuleDetailsByExport } from '@steambrew/client';
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

const RenderComponentType: React.FC<ComponentInterface> = ({ pluginName, object, component, type }) => {
	const settings = window.MILLENNIUM_PLUGIN_SETTINGS_STORE?.[pluginName].settingsStore;
	const items = component?.options?.map((value: string, index: number) => ({ label: value, data: 'componentId' + index }));

	function isFloat(num: number) {
		// First, check if the value is actually a number and not NaN or Infinity
		if (typeof num !== 'number' || !isFinite(num)) {
			return false;
		}
		// Then, check if it's not an integer
		return !Number.isInteger(num);
	}

	const PadFloat = (value: number, digits: number) => {
		const str = value.toString();
		const decimalIndex = str.indexOf('.');
		if (decimalIndex === -1) return str + '.0'.padEnd(digits + 2, '0');
		const decimalPart = str.slice(decimalIndex + 1);
		return str + '0'.repeat(digits - decimalPart.length);
	};

	const [checked, setChecked] = useState<boolean>(component.value);
	const [textValue, setTextValue] = useState<string | number>(isFloat(component.value) ? PadFloat(component.value, 2) : component.value);

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

			setTextValue(isFloat(settings[object]) ? PadFloat(settings[object], 2) : settings[object]);
		} catch (e) {
			console.error('Error parsing number', e);
		}
	}

	const textFieldClass = 'DialogInput DialogInputPlaceholder DialogTextInputBase Focusable';

	// const Slider = findModuleDetailsByExport((m) => m?.toString()?.includes('m_elSlider=e.currentTarget,this.m_rectSlider=this.m_elSlider.getBoundingClientRect()'))
	// console.log(Slider.toString());

	switch (type) {
		case ComponentType.DropDown:
			return (
				<Dropdown
					key={pluginName + object}
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
			console.log(component);

			return (
				// @ts-ignore
				<SliderField
					key={pluginName + object}
					min={component.range[0]}
					max={component.range[1]}
					step={component.step}
					value={textValue as number}
					onChange={(value: any) => {
						settings[object] = value;
						setTextValue(value);
					}}
					showValue={true}
				/>
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

	const components = Object.keys(settings).map((name, index) => {
		const value = settings[name];

		return (
			<Field
				label={value.name}
				description={<Markdown>{value?.desc ?? 'No description yet.'}</Markdown>}
				key={index}
				bottomSeparator={Object.keys(settings).length - 1 === index ? 'none' : 'standard'}
				focusable
			>
				<RenderComponentType pluginName={plugin?.data?.name} object={name} component={value} type={ComponentType[value.type as keyof typeof ComponentType]} />
			</Field>
		);
	});

	return (
		<>
			<div className={`${SettingsModalClass} MillenniumSettings Panel`}>
				<style>{popupStyles}</style>
				{components}
			</div>
		</>
	);
};

export { RenderComponents };
