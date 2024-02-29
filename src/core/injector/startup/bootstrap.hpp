#pragma once

#include <window/api/installer.hpp>
#include <window/interface/globals.h>
#include <format>

#include <stdafx.h>

/**
 * Bootstrapper class for managing theme initialization and updates.
 */
class bootstrapper
{
public:
    /**
     * Parses a local skin directory for skin configuration data.
     *
     * @param entry The directory entry representing the local skin directory.
     * @param buffer A vector to store the parsed JSON data for each skin.
     * @param _checkForUpdates A boolean flag indicating whether to check for updates.
     * @return True if the skin configuration file was successfully parsed and added to the buffer, false otherwise.
     */
    const bool parseLocalSkin(const std::filesystem::directory_entry& entry, std::vector<nlohmann::basic_json<>>& buffer, bool _checkForUpdates);

    /**
     * Initializes the bootstrapper, checking for updates on installed themes and performing necessary actions.
     *
     * @param setCommit A boolean indicating whether to set the new commit information.
     * @param newCommit The new commit information to set if specified.
     */
    const void init(bool setCommit = false, std::string newCommit = "");

private:
};

/**
 * Function to query theme updates asynchronously.
 *
 * This function creates a bootstrapper instance, initializes it to check for theme updates,
 * and returns when the process is completed.
 *
 * @param data Unused parameter (required for thread function signature).
 * @return Always returns 0.
 */
static unsigned long __stdcall queryThemeUpdates(void* data) {
    // Create a unique pointer to a bootstrapper instance.
    std::unique_ptr<bootstrapper> bootstrapper_module = std::make_unique<bootstrapper>();

    // Initialize the bootstrapper to check for theme updates.
    bootstrapper_module->init();

    // Return 0 to indicate successful execution.
    return 0;
}