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

import json
import os
import time
import Millennium
from util.ipc_functions import ChangePluginStatus
from plugins.plugins import find_all_plugins, get_plugin_from_name
from util.logger import logger
import requests
import zipfile
import uuid
import shutil

class PluginInstaller:

    def check_install(self, plugin_name: str) -> bool:
        return True if get_plugin_from_name(plugin_name) is not None else False
    
    def uninstall_plugin(self, pluginName: str):
        try:
            plugin = get_plugin_from_name(pluginName) 

            # check if the plugin is enabled
            if Millennium.is_plugin_enabled(pluginName):
                # disable the plugin before uninstalling it
                Millennium.change_plugin_status([{
                    "plugin_name": pluginName,
                    "enabled": False
                }])

            shutil.rmtree(plugin["path"])
            return True
        except Exception as e:
            logger.error(f"Failed to uninstall {pluginName}: {str(e)}")
            return False
    

    def download_with_progress(self, url, dest_path, progress_callback):
        response = requests.get(url, stream=True)
        total = int(response.headers.get('content-length', 0))
        downloaded = 0

        with open(dest_path, 'wb') as f:
            for chunk in response.iter_content(chunk_size=8192):
                if chunk:
                    f.write(chunk)
                    downloaded += len(chunk)
                    if progress_callback:
                        progress_callback(downloaded, total)


    def extract_zip_with_progress(self, zip_path, extract_to):
        with zipfile.ZipFile(zip_path, 'r') as zip_ref:
            files = zip_ref.infolist()
            total = len(files)

            for i, file in enumerate(files, 1):
                zip_ref.extract(file, extract_to)
                percent = 50 + (45.0 * (i / total))
                self.emit_message("Extracting plugin archive...", percent, False)


    def emit_message(self, status, progress, isComplete):
        json_data = json.dumps({
            "status": status,
            "progress": progress,
            "isComplete": isComplete
        })

        Millennium.call_frontend_method("InstallerMessageEmitter", params=[json_data])


    # Inside your class
    def install_plugin(self, download_url: str, total_size: int) -> bool:
        logger.log(f"Requesting to install plugin -> {download_url}")
        self.emit_message("Starting Plugin Installer...", 0, False)
        time.sleep(2)

        download_path = os.getenv("MILLENNIUM__PLUGINS_PATH")
        
        try:
            zip_path = os.path.join(download_path, uuid.uuid4().hex)

            def progress(downloaded, total):
                percent = (downloaded / total_size) * 100 if total_size else 0
                self.emit_message("Download plugin archive...", 50.0 * (percent / 100.0), False)
                
            self.download_with_progress(download_url, zip_path, progress_callback=progress)
            time.sleep(2)

            self.emit_message("Setting up installed plugin...", 50, False)

            extract_dir = os.path.join(download_path)  # Adjust path
            os.makedirs(extract_dir, exist_ok=True)
            self.extract_zip_with_progress(zip_path, extract_dir)

            self.emit_message("Cleaning up...", 95, False)
            os.remove(zip_path)
            time.sleep(2)

            self.emit_message("Done!", 100, True)
            return json.dumps({'success': True})
        except Exception as e:
            logger.log(f"Failed to install plugin: {e}")
            return json.dumps({'success': False, 'message': str(e)})
