import json
import os
from pathlib import Path
import requests
from typing import List, Dict, Optional
import zipfile
from util.logger import logger

plugin_updater_cache = None

class PluginUpdater:
    def __init__(self):
        self.plugins_path = os.getenv("MILLENNIUM__PLUGINS_PATH")
        if not self.plugins_path:
            raise EnvironmentError("MILLENNIUM__PLUGINS_PATH environment variable not set")

    def _read_metadata(self, plugin_path: Path) -> Optional[Dict[str, str]]:
        """Read metadata.json from a plugin directory if it exists."""
        metadata_path = plugin_path / "metadata.json"
        if not metadata_path.exists():
            return None

        try:
            with open(metadata_path, 'r') as f:
                metadata = json.load(f)
                if "id" in metadata and "commit" in metadata:
                    return {
                        "id": metadata["id"],
                        "commit": metadata["commit"],
                        "name": plugin_path.name
                    }
        except (json.JSONDecodeError, IOError) as e:
            print(f"Failed to read metadata.json for plugin {plugin_path.name}: {e}")
            return None

    def _get_plugin_data(self) -> List[Dict[str, str]]:
        """Get plugin data from all plugins that have metadata.json."""
        plugin_data = []
        
        for plugin_dir in Path(self.plugins_path).iterdir():
            if not plugin_dir.is_dir():
                continue
                
            metadata = self._read_metadata(plugin_dir)
            if metadata:
                plugin_data.append(metadata)
                
        return plugin_data


    def download_and_update_plugin(self, id: str, name: str) -> bool:
        """Download and update a plugin directly into the plugins path."""
        try:
            url = f"https://steambrew.app/api/v1/plugins/download?id={id}&n={name}.zip"
            logger.log(f"Downloading plugin {name} from {url}")

            response = requests.get(url, stream=True)
            response.raise_for_status()
            logger.log(f"Downloaded plugin {name} from {url}")

            # Ensure plugins directory exists
            plugins_path = Path(self.plugins_path)
            plugins_path.mkdir(parents=True, exist_ok=True)

            # Save zip to a temporary path
            temp_zip_path = plugins_path / f"{name}.zip"
            with open(temp_zip_path, 'wb') as f:
                for chunk in response.iter_content(chunk_size=8192):
                    f.write(chunk)

            logger.log(f"Extracting plugin {name} into {plugins_path}")

            # Extract directly into plugins_path, overwriting existing files
            with zipfile.ZipFile(temp_zip_path, 'r') as zip_ref:
                for member in zip_ref.infolist():
                    target_path = plugins_path / member.filename
                    if target_path.exists():
                        if target_path.is_file():
                            target_path.unlink()
                        elif target_path.is_dir():
                            continue  # Preserve folders, or rmtree if needed
                    zip_ref.extract(member, plugins_path)

            temp_zip_path.unlink()  # Clean up the downloaded zip
            logger.log(f"Extracted plugin {name} to {plugins_path}")

            return True

        except Exception as e:
            print(f"Failed to update plugin {name}: {e}")
            return False


    def check_for_updates(self, force: bool = False) -> str:
        """Check for updates for all plugins."""
        global plugin_updater_cache

        if force:
            plugin_updater_cache = None

        if plugin_updater_cache:
            return plugin_updater_cache

        plugin_data = self._get_plugin_data()
        logger.log(f"Checking for updates for {plugin_data} plugins")
        
        try:
            response = requests.post(
                "https://steambrew.app/api/v1/plugins/checkupdates",
                json=plugin_data,
                headers={"Content-Type": "application/json"}
            )
            response.raise_for_status()
            plugin_updater_cache = response.text
            return plugin_updater_cache
        except requests.RequestException as e:
            print(f"Failed to check for updates: {e}")
            return "[]"
