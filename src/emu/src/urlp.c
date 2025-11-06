#include <linux/limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "urlp.h"

/**
 * convert a steamloopback url to abs path and rel path
 * i.e:
 *
 * https://steamloopback.host/chunk~2dcc5aaf7.js?contenthash=0c517757dadc3f7820a3
 * out_path => ~/.steam/steam/steamui/chunk~2dcc5aaf7.js
 * out_short_path => /steamui/chunk~2dcc5aaf7.js
 *
 * @note: caller must free both out_path && out_short_path, ownership is passed to them.
 */
int urlp_path_from_lb(const char* url, char** out_abs, char** out_rel)
{
    const char* path_start = strstr(url, "steamloopback.host");
    if (!path_start || !(path_start = strchr(path_start, '/'))) {
        return -1;
    }

    char* path = strdup(path_start);
    if (!path) return -1;

    char* query = strchr(path, '?');
    if (query) *query = '\0';

    char* file_path = malloc(PATH_MAX);
    if (!file_path) {
        free(path);
        return -1;
    }

    const char* home = getenv("HOME");
    snprintf(file_path, PATH_MAX, "%s/.steam/steam/steamui%s", home ? home : "/home", path);

    *out_abs = file_path;
    *out_rel = path;
    return 0;
}

/**
 * ensure the requested resource is steamloopback host.
 * we do have access to cef_parse_url from the PLT/GOT, but that likely adds more overhead
 * and I don't want to reverse it.
 */
int urlp_should_block_lb_req(cef_string_userfree_t url)
{
    if (!url || !url->str) return 0;
    const char* url_str = url->str;

    /** allow both http and https requests just in case */
    if (strncmp(url_str, "http", 4) != 0) return 0;
    const char* rest = url_str + 4;
    if (*rest == 's') rest++;

    if (strncmp(rest, "://steamloopback.host", 21) != 0) return 0;
    rest += 21;

    if (*rest != '/' && *rest != '\0') return 0;

    const char* query = strchr(url_str, '?');
    const char* last_dot = strrchr(url_str, '.');
    if (!last_dot || (query && last_dot > query)) return 0;

    /** only hook javascript files */
    size_t ext_len = query ? (size_t)(query - last_dot) : strlen(last_dot);
    return ext_len == 3 && strncmp(last_dot, ".js", 3) == 0;
}
