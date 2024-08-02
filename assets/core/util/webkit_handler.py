# This module is intended to keep track on the webkit hooks that are added to the browser
import Millennium # type: ignore

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


def add_browser_css(css_path: str) -> None:
    stack = WebkitStack()
    stack.push(Millennium.add_browser_css(css_path))

def add_browser_js(js_path: str) -> None:
    stack = WebkitStack()
    stack.push(Millennium.add_browser_js(js_path))