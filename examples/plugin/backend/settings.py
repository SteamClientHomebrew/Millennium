from typing import Literal
from MillenniumUtils import CheckBox, DefineSetting, DropDown, NumberTextInput, Settings, FloatSlider, StringTextInput, FloatTextInput, NumberSlider, OnAfterChangeCallback, CallbackLocation
from logger import logger


class PluginSettings(metaclass=Settings):

    @DefineSetting(
        name='CheckBox Example', 
        description='lorem ipsum dolor sit amet, consectetur adipiscing elit',
        style=CheckBox(),
        default=True
    )
    def checkboxInput(self): pass

    @DefineSetting(
        name='Dropdown Example', 
        description='lorem ipsum dolor sit amet, consectetur adipiscing elit',
        style=DropDown(items=["String Value", False, 69]),
        default="String Value"
    )
    def dropDownInput(self): pass

    @DefineSetting(
        name='Float Slider Example', 
        description='lorem ipsum dolor sit amet, consectetur adipiscing elit',
        style=FloatSlider(range=(0.0, 10.0), step=0.5),
        default=0.5
    )
    def floatSliderInput(self): pass

    @DefineSetting(
        name='Number Slider Example', 
        description='lorem ipsum dolor sit amet, consectetur adipiscing elit',
        style=NumberSlider(range=(0, 10), step=1),
        default=5
    )
    def numberSliderInput(self): pass

    @DefineSetting(
        name='Number Text Input Example', 
        description='lorem ipsum dolor sit amet, consectetur adipiscing elit',
        style=NumberTextInput(range=(0, 10000)),
        default=1234
    )
    def numberTextInput(self): pass

    @DefineSetting(
        name='String Text Input Example', 
        description='lorem ipsum dolor sit amet, consectetur adipiscing elit',
        style=StringTextInput(),
        default='Hello World!'
    )
    def stringTextInput(self): pass

    @DefineSetting(
        name='Float Text Input Example', 
        description='lorem ipsum dolor sit amet, consectetur adipiscing elit',
        style=FloatTextInput(range=(0, 10000)),
        default=1234.0
    )
    def floatTextInput(self): pass

    @OnAfterChangeCallback
    def onChange(self, key: str, value: str, prev: str, origin: CallbackLocation):
        """
        This function is triggered whenever a setting is changed, regardless of where the change originates. 
        For example, if you include the following line inside this function:
        
        PluginSettings.numberTextInput += 1

        the function will be immediately be called again, potentially causing an infinite loop if you don't check origin or use flags — so use caution.

        NOTE: There's no need to manually update the frontend, webview, or this setting, as all components are automatically kept in sync.
        """
        logger.log(f"Setting {key} changed to {value} from {origin}.")

