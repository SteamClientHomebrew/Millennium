import sys
import functools

def linux_only(func):
    @functools.wraps(func)
    def wrapper(*args, **kwargs):
        if sys.platform == "linux":
            return func(*args, **kwargs)
    return wrapper

def windows_only(func):
    @functools.wraps(func)
    def wrapper(*args, **kwargs):
        if sys.platform == "win32":
            return func(*args, **kwargs)
    return wrapper
