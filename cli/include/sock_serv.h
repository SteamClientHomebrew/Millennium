enum Millennium {
    GET_LOADED_PLUGINS,
    GET_ENABLED_PLUGINS,
    LIST_PLUGINS,

    GET_PLUGIN_INFO,
    GET_PLUGIN_CONFIG,
    SET_PLUGIN_CONFIG,
    GET_PLUGIN_STATUS,
    SET_PLUGIN_STATUS,
    GET_PLUGIN_LOGS,

    GET_ACTIVE_THEME
};

int send_message(enum Millennium message, char* data);