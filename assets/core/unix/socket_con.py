import base64
import json
import socket
import os
from enum import Enum, auto

from api.plugins import find_all_plugins
from util.logger import logger

import Millennium # type: ignore

SOCKET_PATH = "/tmp/millennium_socket"

class SOCKET_TYPES(Enum):
    GET_LOADED_PLUGINS = 0
    GET_ENABLED_PLUGINS = auto()
    LIST_PLUGINS = auto()
    GET_PLUGIN_INFO = auto()
    GET_PLUGIN_CONFIG = auto()
    SET_PLUGIN_CONFIG = auto()
    GET_PLUGIN_STATUS = auto()
    SET_PLUGIN_STATUS = auto()
    GET_PLUGIN_LOGS = auto()
    GET_ACTIVE_THEME = auto()

def parse_message(message: str) -> tuple:
    return int(message.split("|!|")[0]), message.split("|!|")[1]

def parse_log_types(logs) -> tuple:
    log_count = 0
    warn_count = 0
    error_count = 0

    for log in logs:
        if   log['level'] == 0: log_count   += 1
        elif log['level'] == 1: warn_count  += 1
        elif log['level'] == 2: error_count += 1

    return log_count, warn_count, error_count

def handle_message(encoded_message: str) -> str:
    try:
        type_id, message = parse_message(encoded_message)
        logger.log(f"type: {type_id}, message: {message}")

        if type_id == SOCKET_TYPES.LIST_PLUGINS.value:
            str_response: str = ""
            first_line: bool = True

            logger.log("Getting loaded plugins...")
            data = json.loads(find_all_plugins())

            for plugin in data:
                if not first_line:
                    str_response += "\n\n"
                else:
                    first_line = False

                str_response += f"@@ {plugin['data']['name']} @@\n-- {plugin['data']['common_name']}\n++ {plugin['path']}"

            return str_response
        
        elif type_id == SOCKET_TYPES.GET_PLUGIN_LOGS.value:
            data = json.loads(Millennium.get_plugin_logs()) 

            str_response: str = ""
            first_line: bool = True

            for log in data:
                if not first_line:
                    str_response += "\n\n"
                else:
                    first_line = False

                str_response += f"@@ {log['name']} @@\n"

                log_count, warn_count, error_count = parse_log_types(log['logs'])

                str_response += f"++log-count:{log_count}\n"
                str_response += f"++warn-count:{warn_count}\n"
                str_response += f"++error-count:{error_count}\n"

                for entry in log['logs']:
                    str_response += base64.b64decode(entry['message']).decode('utf-8')

            return str_response

        
        return "Invalid type ID"

    except Exception as e:
        return str(e)

def serve_unix_socket():
    # Ensure old socket file is removed
    if os.path.exists(SOCKET_PATH):
        os.remove(SOCKET_PATH)

    # Create a UNIX socket
    server = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    server.bind(SOCKET_PATH)
    server.listen(1)

    print("Python server waiting for connections...")

    while True:
        conn, _ = server.accept()
        print("Client connected!")

        while True:
            data = conn.recv(1024)
            if not data:
                break

            conn.sendall(handle_message(data.decode()).encode())

        print("Closing client connection...")
        conn.close() 