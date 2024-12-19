# This module is intended to keep track on the webkit hooks that are added to the browser
import json
import os
import Millennium # type: ignore
from util.logger import logger

class WebkitStack:
    _instance = None

    def __new__(cls):
        if cls._instance is None:
            cls._instance = super().__new__(cls)
            cls._instance.stack = []
        return cls._instance

    def is_empty(self):
        return len(self.stack) == 0

    def push(self, item):
        self.stack.append(item)

    def pop(self):
        if not self.is_empty():
            return self.stack.pop()
        else:
            raise IndexError("pop from an empty stack")

    def peek(self):
        if not self.is_empty():
            return self.stack[-1]
        else:
            raise IndexError("peek from an empty stack")

    def size(self):
        return len(self.stack)
    
    def unregister_all(self):
        for hook in self.stack.copy():
            Millennium.remove_browser_module(hook)
            self.stack.remove(hook)

    def remove_all(self):
        self.stack.clear()


def parse_conditional_patches(conditional_patches: dict):
    webkit_items = []

    # Add condition keys into the webkit_items array
    for item, condition in conditional_patches.get('Conditions', {}).items():
        for value, control_flow in condition.get('values', {}).items():
            # Process TargetCss
            target_css = control_flow.get('TargetCss', {})
            if isinstance(target_css, dict):  # Ensure it's a dictionary
                affects = target_css.get('affects', [])
                if isinstance(affects, list):  # Make sure affects is a list
                    for match_string in affects:
                        target_path = target_css.get('src')
                        webkit_items.append({
                            'matchString': match_string,
                            'targetPath': target_path,
                            'fileType': 'TargetCss'
                        })
            

            # Process TargetJs
            target_js = control_flow.get('TargetJs', {})
            if isinstance(target_js, dict):  # Ensure it's a dictionary
                affects = target_js.get('affects', [])
                if isinstance(affects, list):  # Make sure affects is a dictionary
                    for match_string in affects:
                        target_path = target_js.get('src')
                        webkit_items.append({
                            'matchString': match_string,
                            'targetPath': target_path,
                            'fileType': 'TargetJs'
                        })
            

    # Add patch keys into the webkit_items array
    patches = conditional_patches.get('Patches', [])
    for patch in patches:  # Now we assume it's always a list
        if 'TargetCss' in patch:
            target_css = patch['TargetCss']
            if isinstance(target_css, list):
                for target in target_css:
                    webkit_items.append({
                        'matchString': patch['MatchRegexString'],
                        'targetPath': target,
                        'fileType': 'TargetCss'
                    })
            elif isinstance(target_css, str):  # Ensure it's a string if not a list
                webkit_items.append({
                    'matchString': patch['MatchRegexString'],
                    'targetPath': target_css,
                    'fileType': 'TargetCss'
                })

        if 'TargetJs' in patch:
            target_js = patch['TargetJs']
            if isinstance(target_js, list):
                for target in target_js:
                    webkit_items.append({
                        'matchString': patch['MatchRegexString'],
                        'targetPath': target,
                        'fileType': 'TargetJs'
                    })
            elif isinstance(target_js, str):  # Ensure it's a string if not a list
                webkit_items.append({
                    'matchString': patch['MatchRegexString'],
                    'targetPath': target_js,
                    'fileType': 'TargetJs'
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


def add_browser_css(css_path: str, regex=".*") -> None:
    stack = WebkitStack()
    stack.push(Millennium.add_browser_css(css_path, regex))

def add_browser_js(js_path: str, regex=".*") -> None:
    stack = WebkitStack()
    stack.push(Millennium.add_browser_js(js_path, regex))

def add_conditional_data(path: str, data: dict):

    parsed_patches = parse_conditional_patches(data)

    for patch in parsed_patches:
        if patch['fileType'] == 'TargetCss' and patch['targetPath'] is not None and patch['matchString'] is not None:
            add_browser_css(os.path.join(path, patch['targetPath']), patch['matchString'])
        elif patch['fileType'] == 'TargetJs' and patch['targetPath'] is not None and patch['matchString'] is not None:
            add_browser_js(os.path.join(path, patch['targetPath']), patch['matchString'])
