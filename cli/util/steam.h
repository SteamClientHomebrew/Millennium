void ReloadSteam() {
    std::ofstream(SystemIO::GetSteamPath() / "ext" / "reload.flag");
}

void RestartSteam() {
    std::ofstream(SystemIO::GetSteamPath() / "ext" / "restart.flag");
}

void ForceRestartSteam() {
    std::ofstream(SystemIO::GetSteamPath() / "ext" / "restart_force.flag");
}