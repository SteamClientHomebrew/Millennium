from typing import Literal
from MillenniumSettings import CheckBox, DefineSetting, DropDown, NumberTextInput, Settings

class PluginSettings(metaclass=Settings):

    @DefineSetting(
        name='Do frontend call', 
        description='If backend should call the frontend method', 
        style=CheckBox,
        default=True
    )
    def doFrontEndCall(self): pass

    @DefineSetting(
        name='Override webkit document', 
        description='If the webkit document should be overridden', 
        style=CheckBox,
        default=False
    )
    def overrideWebkitDocument(self): pass

    @DefineSetting(
        name='Frontend count', 
        description='Count that gets sent to the backend', 
        style=NumberTextInput,
        range=(0, 10000),
        default=1234
    )
    def frontEndCount(self): pass

    @DefineSetting(
        name='Frontend message', 
        description='Message that gets sent to the backend', 
        style=DropDown[Literal["hello", "hi", "hello again", False, 69]],
        default="hello"
    )
    def frontEndMessage(self): pass