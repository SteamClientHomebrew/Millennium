import os

# On Linux, this lets Millennium use system libraries instead of Steam Runtime libraries. Steams pinned libraries often cause issues with git. 
def use_system_libs(callback):
    old_ld = os.environ.get("LD_LIBRARY_PATH", "")
    clean_ld = ":".join([p for p in old_ld.split(":") if p and "steam-runtime" not in p])
    system_lib_path = "/usr/lib64:/usr/lib"
    os.environ["LD_LIBRARY_PATH"] = f"{system_lib_path}:{clean_ld}"
    try:
        callback()
    finally:
        os.environ["LD_LIBRARY_PATH"] = old_ld