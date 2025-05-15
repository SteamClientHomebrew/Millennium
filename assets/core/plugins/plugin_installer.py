
import json
import os
import time
import Millennium
from plugins.plugins import find_all_plugins
from util.logger import logger
import requests
import zipfile

class PluginInstaller:

    def check_install(self, plugin_name: str) -> bool:
        plugins = json.loads(find_all_plugins())

        for plugin in plugins:
            if "data" in plugin and "name" in plugin["data"] and plugin["data"]["name"] == plugin_name:
                return True
            
        return False
    

    def download_with_progress(self, url, dest_path, progress_callback):
        import requests
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
                percent = 50 + (50.0 * (i / total))
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

        download_path = Millennium.steam_path()
        
        try:
            zip_path = os.path.join(download_path, "plugin.zip")

            def progress(downloaded, total):
                percent = (downloaded / total_size) * 100 if total_size else 0
                self.emit_message("Download plugin archive...", 50.0 * (percent / 100.0), False)
                
            self.download_with_progress(download_url, zip_path, progress_callback=progress)
            time.sleep(2)

            self.emit_message("Setting up installed plugin...", 50, False)

            extract_dir = os.path.join(download_path, "plugin_extracted")  # Adjust path
            os.makedirs(extract_dir, exist_ok=True)
            self.extract_zip_with_progress(zip_path, extract_dir)

            self.emit_message("Done!", 100, True)
            return json.dumps({'success': True})
        except Exception as e:
            logger.log(f"Failed to install plugin: {e}")
            return json.dumps({'success': False, 'message': str(e)})
