## Preload Module

Responsible for bootstrapping millennium by

* Setting up the SharedJSContext (the context window Millennium interacts with)
* Checking for Millennium updates by the `%STEAM_PATH%/ext/update.flag` flag.
  * This update flag is set by user by agreeing to update Millennium, and is not automatically filed. 