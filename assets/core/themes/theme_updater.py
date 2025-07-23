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

from pathlib import Path
import Millennium  # type: ignore
import os, json, shutil, arrow, platform
from datetime import datetime
from themes.accent_color import find_all_themes
from util.use_system import use_system_libs
from util.logger import logger

# Conditional import for Git library based on OS
if platform.system() == "Windows" or platform.system() == "Darwin":
    import pygit2
    GitRepo = pygit2.Repository
elif platform.system() == "Linux":
    from git import Repo as GitRepo  # Use GitPython on Unix

# This module is responsible for updating themes. It does not automatically do so, it is interfaced in the front-end.
class ThemeUpdater:

    def query_themes(self):
        themes = json.loads(find_all_themes())
        needs_copy = False
        update_queue = []
        update_query = []

        for theme in themes:
            path = os.path.join(Millennium.steam_path(), "steamui", "skins", theme["native"])
            try:
                # Use the platform-specific Git repository class
                repo = GitRepo(path)
                update_query.append((theme, repo))
            except (pygit2.GitError if platform.system() == "Windows" else Exception):
                if "github" in theme["data"]:
                    needs_copy = True
                    update_queue.append((theme, path))
            except Exception as e:
                logger.log(f"An exception occurred: {e}")

        if needs_copy:
            source_dir = os.path.join(Millennium.steam_path(), "steamui", "skins")
            destination_dir = os.path.join(
                Millennium.steam_path(),
                "steamui",
                f"skins-backup-{datetime.now().strftime('%Y-%m-%d@%H-%M-%S')}"
            )
            if os.path.exists(destination_dir):
                shutil.rmtree(destination_dir)
            shutil.copytree(source_dir, destination_dir)

        for theme, path in update_queue:
            logger.log(f"upgrading theme to GIT @ {path}")
            if "github" in theme["data"]:
                self.pull_head(path, theme["data"]["github"])

        return update_query

    def construct_post_body(self, update_query):
        post_body = []
        for theme, repo in update_query:
            if "github" not in theme.get("data", {}):
                continue

            github_data = theme["data"]["github"]
            owner = github_data.get("owner")
            repo = github_data.get("repo_name")

            if owner and repo:
                post_body.append({'owner': owner, 'repo': repo})

        return post_body


    def pull_head(self, path: str, data: any) -> None:
        try:
            shutil.rmtree(path)
            repo_url = f'https://github.com/{data["owner"]}/{data["repo_name"]}.git'
            if platform.system() == "Windows":
                pygit2.clone_repository(repo_url, path)
            else:
                GitRepo.clone_from(repo_url, path)
        except Exception as e:
            logger.log(f"An exception occurred: {e}")


    def download_theme_update(self, native: str) -> bool:
        logger.log(f"Updating theme {native}")
        try:
            repo = GitRepo(Path(Millennium.steam_path()) / "steamui" / "skins" / native)
            if platform.system() == "Windows":
                remote = repo.remotes['origin']
                # Enable automatic proxy detection
                remote.fetch(proxy = True)
                latest_commit = repo.revparse_single('origin/HEAD').id
                repo.reset(latest_commit, pygit2.GIT_RESET_HARD)
            else:
                use_system_libs(lambda: repo.remotes.origin.pull())

        except Exception as e:
            logger.log(f"An exception occurred: {e}")
            return False

        logger.log(f"Theme {native} updated successfully.")
        return True


    def check_for_update(self, remote_json, theme, repo_name, repo):
        def has_update(remote_commit: str, repo) -> bool:
            return str(repo[repo.head.target].id if platform.system() == "Windows" else repo.head.commit.hexsha) != str(remote_commit)

        remote = next((item for item in remote_json if item.get("name") == repo_name), None)
        if not remote or not has_update(remote['commit'], repo):
            return []
        
        logger.log(f"Theme {theme['native']} has an update available!")
        return ({
            'message': remote['message'],
            'date':    arrow.get(remote['date']).humanize(),
            'commit':  remote['url'],
            'native':  theme["native"],
            'name':    theme["data"].get("name", theme["native"])
        })


    def get_request_body(self):
        update_query = self.query_themes()
        post_body = self.construct_post_body(update_query)
        if not update_query:
            logger.log("No themes to update!")
            return (None, None)

        return (update_query, post_body)

    def process_updates(self, update_query, remote) -> bool:
        update_list = []
        if not remote:
            logger.log("No remote data available")
            return []

        for theme, repo in update_query:
            if "data" not in theme or "github" not in theme["data"]:
                continue
            repo_name = theme["data"]["github"].get("repo_name")
            checked_theme = self.check_for_update(remote, theme, repo_name, repo)
            if repo_name and checked_theme:
                update_list.append(checked_theme)

        return update_list
