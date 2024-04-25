#include <git2.h>
#include <iostream>
#include <utilities/log.hpp>
#include <boxer/boxer.h>
#include <generic/stream_parser.h>
#include <curl/curl.h>
#include "deps.h"
#include <generic/base.h>

using namespace std::chrono;

namespace dependencies {

    bool is_user_connected() {
        CURL *curl = curl_easy_init();  

        if (!curl) {
            return false;
        }

        curl_easy_setopt(curl, CURLOPT_URL, "https://api.github.com/");
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L); // Set connection timeout to 10 seconds
        curl_easy_setopt(curl, CURLOPT_NOBODY, 1L); // Perform a HEAD request
        // curl_easy_setopt(curl, CURLOPT_WRITEDATA, NULL);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "millennium.patcher/1.0");

        // Perform the HTTP request
        CURLcode res = curl_easy_perform(curl);

        // Cleanup
        curl_easy_cleanup(curl);
        return res == CURLE_OK;
    }

    bool is_up_to_date() {
        // Open the repository
        git_repository *repo = nullptr;
        int error = git_repository_open(&repo, millennium_modules_path.c_str());
        if (error != 0) {
            const git_error *e = git_error_last();
            std::cerr << "Error opening repository: " << e->message << std::endl;
            git_libgit2_shutdown();
            return 1;
        }

        // Open the index (staging area)
        git_index *index = nullptr;
        error = git_repository_index(&index, repo);
        if (error != 0) {
            std::cerr << "Error opening index: " << giterr_last()->message << std::endl;
            git_repository_free(repo);
            git_libgit2_shutdown();
            return 1;
        }

        // Get the count of staged changes
        size_t entry_count = git_index_entrycount(index);
        if (entry_count == 0) {
            std::cout << "No staged changes." << std::endl;
            return true;
        } 

        for (size_t i = 0; i < entry_count; ++i) {
            const git_index_entry *entry = git_index_get_byindex(index, i);
            std::cout << entry->path << std::endl;
            // if (entry->flags & GIT_IDXENTRY_ADDED) {
            //     //std::cout << "(Added)" << std::endl;
            // } else if (entry->flags & GIT_IDXENTRY_UPDATE) {
            //     //std::cout << "(Modified)" << std::endl;
            // } else if (entry->flags & GIT_IDXENTRY_REMOVE) {
            //     //std::cout << "(Deleted)" << std::endl;
            // }
        }

        // Free resources
        git_index_free(index);
        git_repository_free(repo);
        return false;
    }

    void progress_callback(const char *	path, size_t completed_steps, size_t total_steps, void* payload)
    {
        if (completed_steps != 0 && total_steps != 0) {      
            console.log("progress -> {}", ((completed_steps / total_steps) * 100));
        }
    }

    bool clone_millennium_module()
    {
        auto start = steady_clock::now();
        // initialize libgit 
        git_libgit2_init();
        console.log("initialized libgit [{} ms]", duration_cast<milliseconds>(steady_clock::now() - start).count());

        const char* url = "https://github.com/ShadowMonster99/ThemeLoader.git";

        if (!is_user_connected()) {      
            if (!std::filesystem::exists(millennium_modules_path.c_str())) {
                boxer::show("It seems you don't have internet connection or GitHub's API is unreachable. A valid internet connection is required on first startup.", "Error", boxer::Style::Error);
            }
            return false;
        }

        git_repository* repo = nullptr;
        git_clone_options options = GIT_CLONE_OPTIONS_INIT;
        options.checkout_opts.checkout_strategy = GIT_CHECKOUT_FORCE; 
        options.checkout_opts.progress_cb = progress_callback;

        int error = git_repository_open(&repo, millennium_modules_path.c_str());

        if (error == GIT_ENOTFOUND) 
        {
            console.log("cloning millennium modules...");
            // Repository doesn't exist, clone it
            error = git_clone(&repo, url, millennium_modules_path.c_str(), &options);
            if (error == 0) {
                console.log("Successfully cloned millennium frontend modules.");
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
            // Get the remote
            git_remote *remote = nullptr;
            error = git_remote_lookup(&remote, repo, "origin");
            if (error < 0) {
                const git_error *e = git_error_last();
                printf("Error looking up remote: %s\n", e->message);
                git_repository_free(repo);
                git_libgit2_shutdown();
                return false;
            }

            // Fetch the latest changes from the remote
            git_fetch_options fetch_opts = GIT_FETCH_OPTIONS_INIT;
            error = git_remote_fetch(remote, NULL, &fetch_opts, NULL);
            if (error < 0) {
                const git_error *e = git_error_last();
                printf("Error fetching: %s\n", e->message);
                git_remote_free(remote);
                git_repository_free(repo);
                git_libgit2_shutdown();
                return false;
            }

            // Get the latest commit from the remote repository
            git_reference *head_ref = nullptr;
            error = git_repository_head(&head_ref, repo);
            if (error < 0) {
                const git_error *e = git_error_last();
                printf("Error: %s\n", e->message);
                git_remote_free(remote);
                git_repository_free(repo);
                git_libgit2_shutdown();
                return false;
            }

            git_commit *head_commit = nullptr;
            error = git_commit_lookup(&head_commit, repo, git_reference_target(head_ref));
            if (error < 0) {
                const git_error *e = git_error_last();
                printf("Error: %s\n", e->message);
                git_reference_free(head_ref);
                git_remote_free(remote);
                git_repository_free(repo);
                git_libgit2_shutdown();
                return false;
            }

            // Update the working directory to match the latest commit
            git_checkout_options checkout_opts = GIT_CHECKOUT_OPTIONS_INIT;
            checkout_opts.checkout_strategy = GIT_CHECKOUT_FORCE;
            checkout_opts.progress_cb = progress_callback;

            error = git_checkout_tree(repo, (const git_object *)head_commit, &checkout_opts);
            if (error < 0) {
                const git_error *e = git_error_last();
                printf("Error: %s\n", e->message);
                git_commit_free(head_commit);
                git_reference_free(head_ref);
                git_remote_free(remote);
                git_repository_free(repo);
                git_libgit2_shutdown();
                return 1;
            }

            // Cleanup
            git_commit_free(head_commit);
            git_reference_free(head_ref);
            git_remote_free(remote);
        }

        // Check for errors
        if (error < 0) {
            const git_error *e = git_error_last();
            std::cerr << "Error " << error << ": " << e->message << std::endl;
        }

        console.log("module bootstrapper finished [{} ms]", duration_cast<milliseconds>(steady_clock::now() - start).count());

        // Free resources
        git_repository_free(repo);
        git_libgit2_shutdown();
        return error == 0;
    }
}