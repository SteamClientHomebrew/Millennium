import { DefineSetting, MillenniumModuleSettings } from '@steambrew/client';

export class BasicExampleSettings extends MillenniumModuleSettings<BasicExampleSettings> {
    @DefineSetting('Do frontend call', 'If backend should call the frontend method', Boolean)
    public doFrontendCall: boolean = true;

    @DefineSetting('Override webkit document', 'If the webkit document should be overridden', Boolean)
    public overrideWebkitDocument: boolean = false;

    @DefineSetting('Frontend count', 'Count that gets send to the backend', Number)
    public frontendCount: number = 69;

    @DefineSetting('Frontend message', 'Message that gets send to the backend', String)
    public frontendMessage: string = 'Hello World From Frontend!';
}
