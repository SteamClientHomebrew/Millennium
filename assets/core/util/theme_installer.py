import gc
import Millennium # type: ignore
import os, stat, json, shutil, websockets, asyncio
from unix.use_system import unuse_system_libs, use_system_libs
from util.logger import logger

import platform
if platform.system() == "Windows":
    import pygit2
elif platform.system() == "Linux":
    import git

from api.config import cfg
from api.themes import find_all_themes
import pathlib
import asyncio
import threading
import websockets

class WebSocketServer:

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

    def get_theme_from_gitpair(self, repo, owner):
        themes = json.loads(find_all_themes())

        for theme in themes:
            github_data = theme.get("data", {}).get("github", {})
            if github_data.get("owner") == owner and github_data.get("repo_name") == repo:
                return theme

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
        

    def install_theme(self, repo, owner):

        path = os.path.join(Millennium.steam_path(), "steamui", "skins", repo)
        os.makedirs(path, exist_ok=True)

        logger.log(f"Requesting to install theme -> {owner}/{repo} to {path}")

        try:

            if platform.system() == "Windows":
                pygit2.clone_repository(f"https://github.com/{owner}/{repo}.git", path)
            elif platform.system() == "Linux":

                use_system_libs()
                logger.log("Attempting to clone repo from system git")
                git.Repo.clone_from(f"https://github.com/{owner}/{repo}.git", path) 
                unuse_system_libs()

            gc.collect()
            return json.dumps({'success': True})
        except git.CommandError if platform.system() == "Linux" else pygit2.GitError as e:
            logger.error(f"Error cloning repository: {e}")
        except Exception as e:
            return json.dumps({'success': False, 'message': "Failed to clone the theme repository!"})
        

    def handle_set_active_theme(self, repo, owner):
        target_theme = self.get_theme_from_gitpair(repo, owner)

        if target_theme == None:
            return json.dumps({'success': False, 'message': "Couldn't locate the target theme on disk!"})

        cfg.change_theme(target_theme["native"])
        Millennium.call_frontend_method("ReloadMillenniumFrontend")

        return json.dumps({'success': True})

    def __init__(self, host="localhost", port=9123):
        self.host = host
        self.port = port
        self.server = None
        self.loop = None
        self.thread = None
        self.stop_event = asyncio.Event()

    async def handler(self, websocket):
        
        logger.log("Client connected")
        try:
            while not self.stop_event.is_set():
                # Wait for a message from the client
                message = await websocket.recv()
                query = json.loads(message)

                if "type" not in query:
                    return await self.unknown_message(websocket)

                type = query["type"]
                message = query["data"] if "data" in query else None

                if "repo" not in message or "owner" not in message:
                    return await self.unknown_message(websocket)
                
                repo = message["repo"]
                owner = message["owner"]

                action_handlers = {
                    "checkInstall": lambda: self.check_install(repo, owner),
                    "uninstallTheme": lambda: self.uninstall_theme(repo, owner),
                    "installTheme": lambda: self.install_theme(repo, owner),
                    "setActiveTheme": lambda: self.handle_set_active_theme(repo, owner)
                }

                if type in action_handlers:
                    await websocket.send(json.dumps({ "type": type, "data": action_handlers[type]() }))
                else:
                    await self.unknown_message(websocket)
                
                # Send a response back
                await websocket.send(f"Server received: {message}")
        except websockets.ConnectionClosed:
            logger.log("Client disconnected")

    async def start_server(self):
        try:
            # Start WebSocket server
            self.server = await websockets.serve(self.handler, self.host, self.port)
            logger.log(f"Server started on ws://{self.host}:{self.port}")

            # Run server until stop event is set
            await self.stop_event.wait()

            # Close server gracefully
            self.server.close()
            await self.server.wait_closed()
            logger.log("Server stopped")

        except OSError as e:
            if e.errno == 98:  # Errno 98 is typically "Address already in use"
                logger.error(f"Port {self.port} is already in use. Please stop the process using it or choose a different port.")
            else:
                logger.error(f"An unexpected error occurred: {e}")
                
    def _run_loop(self):
        # Create and run the event loop in the thread
        self.loop = asyncio.new_event_loop()
        asyncio.set_event_loop(self.loop)
        self.loop.run_until_complete(self.start_server())

    def start(self):
        # Start server on a new thread
        self.thread = threading.Thread(target=self._run_loop)
        self.thread.start()

    def stop(self):
        # Signal to stop the server and close the loop
        if self.loop and not self.loop.is_closed():
            self.loop.call_soon_threadsafe(self.stop_event.set)
            self.thread.join()
            self.loop.close()
        logger.log("Server thread closed")
