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

import gc
import time
import Millennium # type: ignore
import os, stat, json, shutil
from util.use_system import use_system_libs
from util.logger import logger

import platform
if platform.system() == "Windows" or platform.system() == "Darwin":
    import pygit2
elif platform.system() == "Linux":
    import git

from themes.accent_color import find_all_themes
import asyncio
import websockets

class ThemeInstaller:
    def delete_folder(self, pth):
        for sub in pth.iterdir():
            if sub.is_dir():
                self.delete_folder(sub)
            else:
                sub.unlink()
        pth.rmdir()

    def error_message(self, websocket, message):
        return json.dumps({"type": "error", "message": message})


    async def unknown_message(self, websocket):
        return await websocket.send(json.dumps({"type": "unknown", "message": "unknown message type"}))


    def get_theme_from_gitpair(self, repo, owner, asString=False):
        themes = json.loads(find_all_themes())
        for theme in themes:
            github_data = theme.get("data", {}).get("github", {})
            if github_data.get("owner") == owner and github_data.get("repo_name") == repo:
                return theme if not asString else json.dumps(theme)

        return None


    def check_install(self, repo, owner):
        is_installed = False if self.get_theme_from_gitpair(repo, owner) == None else True
        logger.log(f"Requesting to check install theme: {owner}/{repo} -> {is_installed}")
        return is_installed


    def remove_readonly(self, func, path, exc_info):
        logger.log(f"removing readonly file {exc_info}")

        # Change the file to writable and try the function again
        os.chmod(path, stat.S_IWRITE)
        func(path)


    def uninstall_theme(self, repo, owner):
        logger.log(f"Requesting to uninstall theme -> {owner}/{repo}")

        target_theme = self.get_theme_from_gitpair(repo, owner)
        if target_theme == None:
            return self.error_message("Couldn't locate the target theme on disk!")
        
        path = os.path.join(Millennium.steam_path(), "steamui", "skins", target_theme["native"])
        if not os.path.exists(path):
            return self.error_message("Couldn't locate the target theme on disk!")
        
        try:
            logger.log(f"deleting theme {path}")
            gc.collect()
            shutil.rmtree(path, onerror=self.remove_readonly)
            return json.dumps({'success': True})
        except Exception as e:
            return json.dumps({'success': False, 'message': str(e)})
        
    def emit_message(self, status, progress, isComplete):
        logger.log(f"emitting message {status} {progress} {isComplete}")

        json_data = json.dumps({
            "status": status,
            "progress": progress,
            "isComplete": isComplete
        })

        logger.log(f"emitting message {json_data}")
        Millennium.call_frontend_method("InstallerMessageEmitter", params=[json_data])

    def install_theme(self, repo, owner):

        path = os.path.join(Millennium.steam_path(), "steamui", "skins", repo)
        os.makedirs(path, exist_ok=True)

        self.emit_message("Starting Installer...", 10, False)
        time.sleep(1)

        try:
            self.emit_message("Receiving remote objects...", 40, False)

            if platform.system() == "Windows" or platform.system() == "Darwin":
                pygit2.clone_repository(f"https://github.com/{owner}/{repo}.git", path)

            elif platform.system() == "Linux":
                logger.log("Attempting to clone repo from system git")
                use_system_libs(lambda: git.Repo.clone_from(f"https://github.com/{owner}/{repo}.git", path))

            self.emit_message(f"Done!", 100, True)
            gc.collect()
            return json.dumps({'success': True})
        except git.CommandError if platform.system() == "Linux" else pygit2.GitError as e:
            logger.error(f"Error cloning repository: {e}")
        except Exception as e:
            return json.dumps({'success': False, 'message': "Failed to clone the theme repository!"})
        