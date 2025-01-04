import { DefineSetting, MillenniumModuleSettings } from '@steambrew/client';

export class ComplexSettings extends MillenniumModuleSettings<ComplexSettings> {
    @DefineSetting('Toggle', 'This is an example toggle', Array)
    public toggle: boolean = false;
}
