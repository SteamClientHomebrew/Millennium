

import os

def use_system_libs(callback):
    system_lib_path = "/usr/lib64:/usr/lib"  # Replace this if your libcurl location is different
    os.environ["LD_LIBRARY_PATH"] = f"{system_lib_path}:" + os.environ.get("LD_LIBRARY_PATH", "")
    callback()
    os.environ["LD_LIBRARY_PATH"] = os.environ.get("LD_LIBRARY_PATH", "").replace("/usr/lib64:/usr/lib:", "")
