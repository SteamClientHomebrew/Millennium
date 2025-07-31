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

import enum
import os
import Millennium

import json
import threading
from typing import Callable, Dict, Any, Optional

from config.default_settings import OnMillenniumUpdate, default_config
from util.logger import logger
import platform

class ConfigManager:
    _instance = None
    _instance_lock = threading.Lock()

    def __new__(cls, *args, **kwargs):
        # Thread-safe singleton creation
        if not cls._instance:
            with cls._instance_lock:
                if not cls._instance:
                    cls._instance = super().__new__(cls)
                    # Initialize internal variables here once
                    cls._instance._init_singleton(*args, **kwargs)
        return cls._instance

    def _init_singleton(self, initial_data: Optional[Dict[str, Any]] = None, filename: str = "config.json"):
        self._lock = threading.RLock()
        self._listeners = []  # type: list[Callable[[str, Any, Any], None]]
        self._defaults = {}
        self._data = {}
        self._filename = os.path.join(os.getenv("MILLENNIUM__CONFIG_PATH"), filename)

        self.load_from_file()
        self.set_defaults()
        self.verify_config()

    def get(self, key: str, default: Any = None) -> Any:
        with self._lock:
            return self._data.get(key, self._defaults.get(key, default))

    def set(self, key: str, value: Any, skip_propagation: bool = False):
        with self._lock:
            keys = key.split(".")
            current = self._data
            for k in keys[:-1]:
                if k not in current or not isinstance(current[k], dict):
                    current[k] = {}
                current = current[k]

            last_key = keys[-1]
            old_value = current.get(last_key)

            if old_value != value:
                current[last_key] = value
                self._notify_listeners(key, old_value, value)
                self._auto_save(skip_propagation=skip_propagation)

    def delete(self, key: str):
        with self._lock:
            if key in self._data:
                old_value = self._data[key]
                del self._data[key]
                self._notify_listeners(key, old_value, None)
                logger.log("delete() setting changed")
                self._auto_save()

    def _notify_listeners(self, key: str, old_value: Any, new_value: Any):
        for listener in self._listeners:
            try:
                listener(key, old_value, new_value)
            except Exception as e:
                print(f"Listener error: {e}")

    def register_listener(self, listener: Callable[[str, Any, Any], None]):
        with self._lock:
            if listener not in self._listeners:
                self._listeners.append(listener)

    def unregister_listener(self, listener: Callable[[str, Any, Any], None]):
        with self._lock:
            if listener in self._listeners:
                self._listeners.remove(listener)

    def load_from_file(self):
        if not os.path.exists(self._filename):
            logger.log(f"Config file not found. Creating new config file: {self._filename}")
            try:
                with open(self._filename, "w", encoding="utf-8") as f:
                    json.dump({}, f, indent=2, default=self.custom_encoder)
            except (IOError, PermissionError) as e:
                logger.log(f"Error creating config file: {e}")
                self._data = {}
                return
            
        try:
            with open(self._filename, "r", encoding="utf-8") as f:
                try:
                    data = json.load(f)
                except json.JSONDecodeError:
                    logger.log("Invalid JSON in config file. Creating new config.")
                    data = {}
                    # Write the new empty config in a separate operation
                    with open(self._filename, "w", encoding="utf-8") as f:
                        json.dump(data, f, indent=2, default=self.custom_encoder)
        except (IOError, PermissionError) as e:
            logger.log(f"Error accessing config file: {e}")
            data = {}
            
        with self._lock:
            self._data = data
        for k, v in data.items():
            self._notify_listeners(k, None, v)

    def save_to_file(self, skip_propagation: bool = False):
        data_copy = self._data.copy()
        
        with open(self._filename, "w", encoding="utf-8") as f:
            json.dump(data_copy, f, indent=2, default=self.custom_encoder)

        if not skip_propagation:
            try:
                Millennium.call_frontend_method("OnBackendConfigUpdate", 
                    params=[json.dumps(data_copy, default=self.custom_encoder)])
            except ConnectionError as e:
                pass

    def update(self, update_dict: Dict[str, Any]):
        with self._lock:
            changed = False
            for k, v in update_dict.items():
                old_value = self._data.get(k, None)
                if old_value != v:
                    self._data[k] = v
                    self._notify_listeners(k, old_value, v)
                    changed = True
            if changed:
                self._auto_save()

    def set_default(self, key: str, value: Any):
        with self._lock:
            self._defaults[key] = value

    def set_all(self, config: Dict[str, Any], skip_propagation: bool = False):
        with self._lock:
            if isinstance(config, str):
                try:
                    config = json.loads(config)
                except json.JSONDecodeError:
                    print(f"Invalid JSON format: {config}")
                    config = {}

            if not isinstance(config, dict) or not config:
                return

            changed = False
            for key, new_value in config.items():
                old_value = self._data.get(key)
                if old_value != new_value:
                    self._data[key] = new_value
                    self._notify_listeners(key, old_value, new_value)
                    changed = True
            if changed:
                self._auto_save(skip_propagation=skip_propagation)

    def custom_encoder(self, obj):
        if isinstance(obj, enum.Enum):
            return obj.value
        elif isinstance(obj, bytes):
            return obj.decode("utf-8", errors="replace")
        elif hasattr(obj, "__dict__"):
            return vars(obj)
        else:
            return str(obj)  # fallback

    def _sanitize(self) -> Any:
        with self._lock:
            # Remove any non-serializable objects
            return json.loads(json.dumps(self._data, default=self.custom_encoder))

    
    def get_all(self) -> Dict[str, Any]:
        with self._lock:
            result = self._defaults.copy()
            result.update(self._data)
            return self._sanitize()


    def __getitem__(self, key: str) -> Any:
        if "." in key:
            keys = key.split(".")
            current = self._data

            for k in keys:
                if isinstance(current, dict) and k in current:
                    current = current[k]
                else:
                    raise KeyError(key)
            return current
        else:
            return self._data[key]
        

    def __setitem__(self, key: str, value: Any):
        self.set(key, value)

    def __delitem__(self, key: str):
        self.delete(key)

    def __contains__(self, key: str):
        with self._lock:
            keys = key.split(".")
            current = self._data
            for k in keys:
                if isinstance(current, dict) and k in current:
                    current = current[k]
                else:
                    return False
            return True

    def __repr__(self):
        return f"ConfigManager({self.get_all()})"

    def _auto_save(self, skip_propagation: bool = False):
        self.save_to_file(skip_propagation=skip_propagation)

    def set_defaults(self, prefix: str = ""):
        def _merge(current: dict, incoming: dict, path: str):
            for key, value in incoming.items():
                full_key = f"{path}.{key}" if path else key
                if isinstance(value, dict):
                    if key not in current or not isinstance(current[key], dict):
                        current[key] = {}
                    _merge(current[key], value, full_key)
                else:
                    if key not in current:
                        current[key] = value
                        self._notify_listeners(full_key, None, value)

        with self._lock:
            _merge(self._data, default_config, "")
            logger.log("Setting default settings")
            self._auto_save()

    def verify_config(self):
        logger.log("Verifying configuration settings...")

        with self._lock:
            system = platform.system()
            if system not in ("Linux", "Darwin"):
                logger.log("skipping verify_config on non-Linux/macOS system")
                return 
            
            try:
                on_update = self["general.onMillenniumUpdate"]
                logger.log(f"Current onMillenniumUpdate setting: {on_update}")

                if on_update == OnMillenniumUpdate.AUTO_INSTALL.value:
                    logger.warn("OnMillenniumUpdate is set to AUTO_INSTALL on a non-Windows system. Changing to NOTIFY.")
                    self.set("general.onMillenniumUpdate", OnMillenniumUpdate.NOTIFY.value)

            except KeyError as e:
                logger.error(f"KeyError in verify_config: {e}")

# Helper function to get the singleton instance easily
def get_config() -> ConfigManager:
    return ConfigManager()
