import json
import os
from pathlib import Path
import requests
from typing import List, Dict, Optional

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
                        "commit": metadata["commit"]
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

    def check_for_updates(self) -> str:
        """Check for updates for all plugins."""
        plugin_data = self._get_plugin_data()
        
        try:
            response = requests.post(
                "http://localhost:3000/api/v1/plugins/checkupdates",
                json=plugin_data,
                headers={"Content-Type": "application/json"}
            )
            response.raise_for_status()
            return response.text
        except requests.RequestException as e:
            print(f"Failed to check for updates: {e}")
            return "{}"
