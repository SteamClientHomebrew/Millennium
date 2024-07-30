void ReloadSteam() {
    std::ofstream(SystemIO::GetSteamPath() / "ext" / "reload.flag");
}
