void RestartSteam() {
    std::ofstream(SystemIO::GetSteamPath() / "ext" / "restart.flag");
}
