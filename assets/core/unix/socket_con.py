import base64
import json
import socket
import os
from enum import Enum, auto

from plugins.plugins import find_all_plugins
from util.logger import logger

import Millennium  # type: ignore

SOCKET_PATH = "/tmp/millennium_socket"

class SocketTypes(Enum):
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

class MillenniumSocketServer:
    def __init__(self, socket_path=SOCKET_PATH):
        self.socket_path = socket_path
        self.server = None
        self.running = False

    def parse_message(self, message: str) -> tuple:
        return int(message.split("|!|")[0]), message.split("|!|")[1]

    def parse_log_types(self, logs) -> tuple:
        log_count = sum(1 for log in logs if log['level'] == 0)
        warn_count = sum(1 for log in logs if log['level'] == 1)
        error_count = sum(1 for log in logs if log['level'] == 2)
        return log_count, warn_count, error_count

    def handle_message(self, encoded_message: str) -> str:
        try:
            type_id, message = self.parse_message(encoded_message)
            logger.log(f"type: {type_id}, message: {message}")

            if type_id == SocketTypes.LIST_PLUGINS.value:
                str_response = ""
                first_line = True

                logger.log("Getting loaded plugins...")
                data = json.loads(find_all_plugins())

                for plugin in data:
                    if not first_line:
                        str_response += "\n\n"
                    else:
                        first_line = False
                    str_response += f"@@ {plugin['data']['name']} @@\n-- {plugin['data']['common_name']}\n++ {plugin['path']}"

                return str_response

            elif type_id == SocketTypes.GET_PLUGIN_LOGS.value:
                data = json.loads(Millennium.get_plugin_logs())
                str_response = ""
                first_line = True

                for log in data:
                    if not first_line:
                        str_response += "\n\n"
                    else:
                        first_line = False
                    str_response += f"@@ {log['name']} @@\n"
                    log_count, warn_count, error_count = self.parse_log_types(log['logs'])
                    str_response += f"++log-count:{log_count}\n"
                    str_response += f"++warn-count:{warn_count}\n"
                    str_response += f"++error-count:{error_count}\n"
                    for entry in log['logs']:
                        str_response += base64.b64decode(entry['message']).decode('utf-8')
                return str_response

            return "Invalid type ID"

        except Exception as e:
            return str(e)

    def serve(self):
        self.is_server_running = True

        if os.path.exists(self.socket_path):
            os.remove(self.socket_path)

        self.server = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        self.server.bind(self.socket_path)
        self.server.listen(1)
        self.server.settimeout(1.0)  # Set a timeout to allow checking `self.running`
        self.running = True

        logger.log("Python server waiting for connections...")

        while self.running:
            try:
                conn, _ = self.server.accept()
            except socket.timeout as e:
                continue  # Just retry if no connection was received
            except OSError as e:
                break

            logger.log("Client connected!")

            while self.running:
                try:
                    data = conn.recv(1024)
                    if not data:
                        break
                    conn.sendall(self.handle_message(data.decode()).encode())
                    conn.close()
                except socket.error:
                    break  # Break if an error occurs

            logger.log("Closing client connection...")
            conn.close()

        self.server.close()
        if os.path.exists(self.socket_path):
            os.remove(self.socket_path)
        logger.log("Socket server stopped.")

        self.is_server_running = False

    def stop(self):
        self.running = False
        if self.server:
            self.server.close()
        if os.path.exists(self.socket_path):
            os.remove(self.socket_path)

        while self.is_server_running:
            pass

        logger.log("UNIX socket server stopped.")
