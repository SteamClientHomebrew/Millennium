#include <git2.h>
#include <iostream>
#include <utilities/log.hpp>
#include <boxer/boxer.h>
#include <generic/stream_parser.h>
#include "deps.h"
#include <generic/base.h>

using namespace std::chrono;

namespace dependencies {

    /**
     * @brief fetch head reference callback 
     * 
     * copies the oid of the reference into the payload to use in the updater
     * @return success [C style bool]
    */
    int fetchhead_ref_cb(const char *name, const char *url,
       const git_oid *oid, unsigned int is_merge, void *payload)
    {
        if (is_merge)
        {
            console.log_item("fetchhead_ref_cb", "caught type [merge]");
            git_oid_cpy((git_oid *)payload, oid);
        }
        return 0;
    }

    const int clone_repository(git_repository* repo, std::string package_path, std::string remote_object) {
        int error;
        git_clone_options options = GIT_CLONE_OPTIONS_INIT;
        options.checkout_opts.checkout_strategy = GIT_CHECKOUT_FORCE; 
 
        // can't clone into a non-empty directory
        if (std::filesystem::exists(package_path) && !std::filesystem::is_empty(package_path)) {
            console.log_item("status", "flusing module path...");
            std::uintmax_t removed_count = std::filesystem::remove_all(package_path);
        }
        else {
            console.log_item("status", "ready to clone module...");
        }

        console.log_item("status", "cloning modules...");
        error = git_clone(&repo, remote_object.c_str(), package_path.c_str(), &options);

        if (error != 0) {
            const git_error* e = git_error_last();
            const std::string message = fmt::format("Error cloning frontend modules -> {}", e->message);
            console.log_item("status", fmt::format("failed to clone [{}]", e->message), true);

            boxer::show(message.c_str(), "Fatal Error", boxer::Style::Error);
            return error;
        }

        //console.log_item("status", "finished module audit!", true);
        return error;
    }

    const int fetch_head(git_repository* repo, std::string package_path) {
        int error;
        console.log_item("status", "checking for module updates...");
        // Fetch the latest changes from the remote
        git_remote *remote;
        error = git_remote_lookup(&remote, repo, "origin");
        
        if (error < 0) {
            console.log_item("error", fmt::format("failed lookup -> {}", git_error_last()->message));
            return 0; 
        }

        git_fetch_options options = GIT_FETCH_OPTIONS_INIT;
            error = git_remote_fetch(remote,
                                    NULL, /* refspecs, NULL to use the configured ones */
                                    &options, /* options, empty for defaults */
                                    "pull"); /* reflog mesage, usually "fetch" or "pull", you can leave it NULL for "fetch" */
        
        if (error < 0) {
            const git_error * last_error = git_error_last();
            console.log_item("error", fmt::format("failed fetch -> -> klass: {}, message: {}", last_error->klass, last_error->message));

            // Couldn't connect to GitHub, and modules dont already exist.
            if (last_error->klass == GIT_ERROR_NET && !std::filesystem::exists(package_path.c_str())) 
            {
                boxer::show("It seems you don't have internet connection or GitHub's API is unreachable. A valid internet connection is required to setup Millennium.", "Error", boxer::Style::Error);
            }
            return 1; 
        }

        git_oid branchOidToMerge;
        git_annotated_commit *their_heads[1];

        git_repository_fetchhead_foreach(repo, fetchhead_ref_cb, &branchOidToMerge);
        error = git_annotated_commit_lookup(&their_heads[0], repo, &branchOidToMerge);

        if (error < 0) {
            console.err("error looking up annotated commit -> {}", git_error_last()->message);
            return 1; 
        }

        git_merge_analysis_t anout;
        git_merge_preference_t pout;

        console.log_item("status", "analyzing repository information...");
        error = git_merge_analysis(&anout, &pout, repo, (const git_annotated_commit **) their_heads, 1);

        if (error < 0) {
            console.log_item("error", fmt::format("couldn't analyze -> {}", git_error_last()->message));
            return 1; 
        }

        if (anout & GIT_MERGE_ANALYSIS_UP_TO_DATE) 
        {
            console.log_item("status", "modules seem to be up-to-date!");

            git_annotated_commit_free(their_heads[0]);
            git_repository_state_cleanup(repo);
            git_remote_free(remote);

            return 0;
        } 
        else if (anout & GIT_MERGE_ANALYSIS_FASTFORWARD) 
        {
            console.log_item("status", "fast-forwarding modules...");

            git_reference *ref;
            git_reference *newref;

            const char *name = "refs/heads/master";

            if (git_reference_lookup(&ref, repo, name) == 0)
                git_reference_set_target(&newref, ref, &branchOidToMerge, "pull: Fast-forward");

            git_reset_from_annotated(repo, their_heads[0], GIT_RESET_HARD, NULL);

            git_reference_free(ref);
            git_repository_state_cleanup(repo);
        }

        git_annotated_commit_free(their_heads[0]);
        git_repository_state_cleanup(repo);
        git_remote_free(remote);
        return error;
    }

    /**
     * @brief clones and manages index version of millennium modules
     * 
     * clones the __builtin__ modules if they don't exist on disk, and if they do exist
     * they are checked against upstream and pulled if there were commits/changes on the remote. 
     * 
     * @return success status 
    */
    bool audit_package(std::string common_name, std::string package_path, std::string remote_object)
    {
        const auto start = steady_clock::now();
        git_libgit2_init();
        
        console.head(fmt::format("libgit - {} [{} ms]", common_name, duration_cast<milliseconds>(steady_clock::now() - start).count()));
        console.log_item("modules", package_path);

        git_repository* repo = nullptr;
        int error = git_repository_open(&repo, package_path.c_str());

        switch (error) {
            case GIT_ENOTFOUND: {
                console.log_item("modules", "repo not found...");
                error = clone_repository(repo, package_path, remote_object);
                break;
            }
            case 0: {
                console.log_item("modules", "fetching head for updates...");
                error = fetch_head(repo, package_path);
                break;
            } 
            default: {
                const git_error *e = git_error_last();
                console.log_item("error", fmt::format("couldn't evaluate repo -> {}", e->message));
            }
        }

        console.log_item("status", fmt::format("done: [{} ms]; ec: [{}]", duration_cast<milliseconds>(steady_clock::now() - start).count(), error), true);
        // Free resources
        git_repository_free(repo);
        git_libgit2_shutdown();
        return error == 0;
    }
}