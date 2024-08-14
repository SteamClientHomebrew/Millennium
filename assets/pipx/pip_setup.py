from logger import logger

def verify_pip():
    try:
        from pip._internal import main
    except ImportError:
        logger.error("PIP was not found on your Millennium installation. Please reinstall...")