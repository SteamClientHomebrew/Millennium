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
        console.log("called [fetchhead_ref_cb]");

        if (is_merge)
        {
            console.log("caught type [merge]");
            git_oid_cpy((git_oid *)payload, oid);
        }
        return 0;
    }


    /**
     * @brief clones and manages index version of millennium modules
     * 
     * clones the __builtin__ modules if they don't exist on disk, and if they do exist
     * they are checked against upstream and pulled if there were commits/changes on the remote. 
     * 
     * @return success status 
    */
    bool clone_millennium_module()
    {
        const auto start = steady_clock::now();
        // initialize libgit 
        git_libgit2_init();
        
        console.log("initialized libgit [{} ms]", duration_cast<milliseconds>(steady_clock::now() - start).count());

        console.log("modules path -> {}", millennium_modules_path);

        git_repository* repo = nullptr;
        git_clone_options options = GIT_CLONE_OPTIONS_INIT;
        options.checkout_opts.checkout_strategy = GIT_CHECKOUT_FORCE; 
 
        int error = git_repository_open(&repo, millennium_modules_path.c_str());

        if (error == GIT_ENOTFOUND) 
        {
            console.log("cloning millennium modules...");
            // Repository doesn't exist, clone it
            error = git_clone(&repo, modules_repo, millennium_modules_path.c_str(), &options);
            if (error == 0) {
                console.log("successfully cloned millennium frontend modules.");
            } else {
                const git_error* e = git_error_last();
                const std::string message = fmt::format("Error cloning frontend modules -> {}", e->message);

                console.err(message);
                boxer::show(message.c_str(), "Fatal Error", boxer::Style::Error);
            }
        } 
        else if (error == 0) 
        {
            console.log("checking for module updates...");
            // Fetch the latest changes from the remote
            git_remote *remote;
            error = git_remote_lookup(&remote, repo, "origin");
            
            if (error < 0) {
                console.err("error looking up remote -> {}", git_error_last()->message);
                return 0; 
            }


            git_fetch_options options = GIT_FETCH_OPTIONS_INIT;
                error = git_remote_fetch(remote,
                                        NULL, /* refspecs, NULL to use the configured ones */
                                        &options, /* options, empty for defaults */
                                        "pull"); /* reflog mesage, usually "fetch" or "pull", you can leave it NULL for "fetch" */
            
            if (error < 0) {

                const git_error * last_error = git_error_last();
                console.err("error fetching remote -> klass: {}, message: {}", last_error->klass, last_error->message);

                // Couldn't connect to GitHub, and modules dont already exist.
                if (last_error->klass == GIT_ERROR_NET && !std::filesystem::exists(millennium_modules_path.c_str())) 
                {
                    boxer::show("It seems you don't have internet connection or GitHub's API is unreachable. A valid internet connection is required on first startup.", "Error", boxer::Style::Error);
                }

                return false; 
            }

            git_oid branchOidToMerge;

            git_repository_fetchhead_foreach(repo, fetchhead_ref_cb, &branchOidToMerge);
            git_annotated_commit *their_heads[1];

            error = git_annotated_commit_lookup(&their_heads[0], repo, &branchOidToMerge);

            if (error < 0) {
                console.err("error looking up annotated commit -> {}", git_error_last()->message);
                return false; 
            }

            git_merge_analysis_t anout;
            git_merge_preference_t pout;

            console.log("analysing repository information");

            error = git_merge_analysis(&anout, &pout, repo, (const git_annotated_commit **) their_heads, 1);

            if (error < 0) {
                console.err("error analysing merge -> {}", git_error_last()->message);
                return false; 
            }

            if (anout & GIT_MERGE_ANALYSIS_UP_TO_DATE) 
            {
                console.log("modules seem to be up-to-date!");

                git_annotated_commit_free(their_heads[0]);
                git_repository_state_cleanup(repo);
                git_remote_free(remote);

                return true;
            } 
            else if (anout & GIT_MERGE_ANALYSIS_FASTFORWARD) 
            {
                console.log("fast-forwarding modules...");

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
        }

        // Check for errors
        if (error < 0) {
            const git_error *e = git_error_last();
            console.err("error cloing modules -> {}", e->message);
        }

        console.log("module bootstrapper finished [{} ms]", duration_cast<milliseconds>(steady_clock::now() - start).count());

        // Free resources
        git_repository_free(repo);
        git_libgit2_shutdown();
        return error == 0;
    }
}