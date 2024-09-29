import gc
import Millennium # type: ignore
import os, stat, json, shutil, websockets, asyncio, pygit2
from api.config import cfg
from api.themes import find_all_themes
import pathlib

def delete_folder(pth):
    for sub in pth.iterdir():
        if sub.is_dir():
            delete_folder(sub)
        else:
            sub.unlink()
    pth.rmdir()

def error_message(websocket, message):
    return json.dumps({"type": "error", "message": message})

async def unknown_message(websocket):
    return await websocket.send(json.dumps({"type": "unknown", "message": "unknown message type"}))

def get_theme_from_gitpair(repo, owner):
    themes = json.loads(find_all_themes())

    for theme in themes:
        github_data = theme.get("data", {}).get("github", {})
        if github_data.get("owner") == owner and github_data.get("repo_name") == repo:
            return theme

    return None

def check_install(repo, owner):
    return False if get_theme_from_gitpair(repo, owner) == None else True

def remove_readonly(func, path, exc_info):
    print(f"removing readonly file {exc_info}")

    # Change the file to writable and try the function again
    os.chmod(path, stat.S_IWRITE)
    func(path)

def uninstall_theme(repo, owner):
    target_theme = get_theme_from_gitpair(repo, owner)
    if target_theme == None:
        return error_message("Couldn't locate the target theme on disk!")
    
    path = os.path.join(Millennium.steam_path(), "steamui", "skins", target_theme["native"])
    if not os.path.exists(path):
        return error_message("Couldn't locate the target theme on disk!")
    
    try:
        print(f"deleting theme {path}")
        gc.collect()
        shutil.rmtree(path, onerror=remove_readonly)
        return json.dumps({'success': True})
    except Exception as e:
        return json.dumps({'success': False, 'message': str(e)})
    

def install_theme(repo, owner):
    path = os.path.join(Millennium.steam_path(), "steamui", "skins", repo)
    os.makedirs(path, exist_ok=True)

    try:
        pygit2.clone_repository(f"https://github.com/{owner}/{repo}.git", path)
        gc.collect()
        return json.dumps({'success': True})
    except Exception as e:
        return json.dumps({'success': False, 'message': "Failed to clone the theme repository!"})
    

def handle_set_active_theme(repo, owner):
    target_theme = get_theme_from_gitpair(repo, owner)

    if target_theme == None:
        return json.dumps({'success': False, 'message': "Couldn't locate the target theme on disk!"})

    cfg.change_theme(target_theme["native"])
    Millennium.call_frontend_method("ReloadMillenniumFrontend")

    return json.dumps({'success': True})

async def echo(websocket, path):
    async for message in websocket:
        query = json.loads(message)

        if "type" not in query:
            return await unknown_message(websocket)

        type = query["type"]
        message = query["data"] if "data" in query else None

        if "repo" not in message or "owner" not in message:
            return await unknown_message(websocket)
        
        repo = message["repo"]
        owner = message["owner"]

        action_handlers = {
            "checkInstall": lambda: check_install(repo, owner),
            "uninstallTheme": lambda: uninstall_theme(repo, owner),
            "installTheme": lambda: install_theme(repo, owner),
            "setActiveTheme": lambda: handle_set_active_theme(repo, owner)
        }

        if type in action_handlers:
            await websocket.send(json.dumps({ "type": type, "data": action_handlers[type]() }))
        else:
            await unknown_message(websocket)


async def serve_websocket():
    while True:
        try:
            async with websockets.serve(echo, "localhost", 9123):
                await asyncio.Future()  # run forever
        except Exception as e:
            print(f"ipc socket closed on exception: {e}")
            print("restarting websocket...")
            await asyncio.sleep(5)  # Wait for 5 seconds before reconnecting

def start_websocket_server():
    asyncio.set_event_loop(asyncio.new_event_loop())
    asyncio.get_event_loop().run_until_complete(serve_websocket())

    start_websocket_server()
