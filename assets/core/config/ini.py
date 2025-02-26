import configparser
import os
import Millennium # type: ignore
from plat_spec.main import config_path

class IniConfig:

    config_path = config_path

    @staticmethod
    def get_config(header: str, key: str, fallback: str) -> str:
        config = configparser.ConfigParser()

        with open(IniConfig.config_path, 'r') as enabled:
            config.read_file(enabled)
        
        return config.get(header, key, fallback=fallback)
    
    @staticmethod
    def set_config(header: str, key: str, value: str) -> None:
        config = configparser.ConfigParser()

        with open(IniConfig.config_path, 'r') as enabled:
            config.read_file(enabled)
        
        config[header][key] = value

        with open(IniConfig.config_path, 'w') as enabled:
            config.write(enabled)