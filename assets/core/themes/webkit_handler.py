# ==================================================
#   _____ _ _ _             _                     
#  |     |_| | |___ ___ ___|_|_ _ _____           
#  | | | | | | | -_|   |   | | | |     |          
#  |_|_|_|_|_|_|___|_|_|_|_|_|___|_|_|_|          
# 
# ==================================================
# 
# Copyright (c) 2025 Project Millennium
# 
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
# 
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
# 
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

# This module is intended to keep track on the webkit hooks that are added to the browser
import Millennium, sys, os, json, traceback
from config.manager import get_config
from util.logger import logger

class WebkitHookStore:
    _instance = None

    def __new__(cls):
        if cls._instance is None:
            cls._instance = super().__new__(cls)
            cls._instance.stack = []
        return cls._instance

    def push(self, item):
        self.stack.append(item)

    def unregister_all(self):
        for hook in self.stack.copy():
            Millennium.remove_browser_module(hook)
            self.stack.remove(hook)


def parse_conditional_patches(conditional_patches: dict, theme_name: str):
    webkit_items = []
    theme_conditions = get_config()["themes.conditions"]

    # Add condition keys into the webkit_items array
    for item, condition in conditional_patches.get('Conditions', {}).items():
        for value, control_flow in condition.get('values', {}).get(theme_conditions[theme_name][item], {}).items():
            if isinstance(control_flow, dict): 
                affects = control_flow.get('affects', [])
                if isinstance(affects, list): 
                    for match_string in affects:
                        target_path = control_flow.get('src')
                        webkit_items.append({
                            'matchString': match_string,
                            'targetPath': target_path,
                            'fileType': value
                        })
            

    patches = conditional_patches.get('Patches', [])
    for patch in patches:  
        for inject_type in ['TargetCss', 'TargetJs']:
            if inject_type in patch:
                target_css = patch[inject_type]
                if isinstance(target_css, str):
                    target_css = [target_css]
                for target in target_css:
                    webkit_items.append({
                        'matchString': patch['MatchRegexString'],
                        'targetPath': target,
                        'fileType': inject_type
                    })

    # Remove duplicates
    seen = set()
    unique_webkit_items = []
    for item in webkit_items:
        identifier = (item['matchString'], item['targetPath'])
        if identifier not in seen:
            seen.add(identifier)
            unique_webkit_items.append(item)

    return unique_webkit_items

conditional_patches = []

def add_browser_css(css_path: str, regex=".*") -> None:
    stack = WebkitHookStore()
    stack.push(Millennium.add_browser_css(css_path, regex))

def add_browser_js(js_path: str, regex=".*") -> None:
    stack = WebkitHookStore()
    stack.push(Millennium.add_browser_js(js_path, regex))

def remove_all_patches() -> None:
    for target_path, module_id in conditional_patches:
        if module_id is not None:  # Check for None
            Millennium.remove_browser_module(module_id)
    conditional_patches.clear()

def add_conditional_data(path: str, data: dict, theme_name: str):
    try:
        remove_all_patches()
        parsed_patches = parse_conditional_patches(data, theme_name)
        for patch in parsed_patches:
            if patch['fileType'] == 'TargetCss' and patch['targetPath'] is not None and patch['matchString'] is not None:
                target_path = os.path.join(path, patch['targetPath'])
                conditional_patches.append((target_path, add_browser_css(target_path, patch['matchString'])))

            elif patch['fileType'] == 'TargetJs' and patch['targetPath'] is not None and patch['matchString'] is not None:
                target_path = os.path.join(path, patch['targetPath'])
                conditional_patches.append((target_path, add_browser_js(target_path, patch['matchString'])))

    except Exception as e:
        logger.log(f"Error adding conditional data: {e}")
        # log error to stderr
        sys.stderr.write(traceback.format_exc())