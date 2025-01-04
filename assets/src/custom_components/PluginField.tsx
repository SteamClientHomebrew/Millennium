import { ComponentProps } from './PluginSettings';
import React, { ChangeEventHandler, ReactElement, useState } from 'react';
import { Field, SettingType, TextField, Toggle } from '@steambrew/client';

export const PluginField: React.FC<ComponentProps> = ({settingsModule, propertyKey, metadata}) => {
    const [value, setValue] = useState(settingsModule[propertyKey]);

    const RenderToggle: React.FC<ComponentProps> = ({settingsModule, propertyKey}) => {
        const onCheckChange = (enabled: boolean) => {
            settingsModule[propertyKey] = enabled;
            setValue(enabled);
        };

        return <Toggle value={value} onChange={onCheckChange}/>;
    };

    const RenderTextField: React.FC<ComponentProps> = ({settingsModule, propertyKey, metadata}) => {
        const onValueChange: ChangeEventHandler<HTMLInputElement> = (e) => {
            let newValue: string | number = e.target.value;
            if (metadata.type === SettingType.NumberInput) {
                newValue = parseInt(e.target.value);
            }

            if (newValue !== value) {
                settingsModule[propertyKey] = newValue;
                setValue(newValue);
            }
        };

        return <TextField
            value={value}
            onChange={onValueChange}
            // Focus on click otherwise we can't focus the textfield because of how showModal works
            onClick={(e) => (e.target as HTMLInputElement).focus()}
        />;
    };


    let component: ReactElement;
    switch (metadata.type) {
        case SettingType.Toggle:
            component = <RenderToggle settingsModule={settingsModule} propertyKey={propertyKey} metadata={metadata}/>;
            break;
        case SettingType.NumberInput:
        case SettingType.TextInput:
            component = <RenderTextField settingsModule={settingsModule} propertyKey={propertyKey} metadata={metadata}/>;
            break;
        default:
            component = <span style={{color: 'crimson'}}>Unsupported setting type "{metadata.type}"</span>;
            break;
    }

    return (
        <Field label={metadata.name} description={metadata.description}>
            {component}
        </Field>
    );
};
