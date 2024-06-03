#include <git2.h>
#include <iostream>
#include <utilities/log.hpp>
#include <boxer/boxer.h>
#include <generic/stream_parser.h>
#include "deps.h"
#include <generic/base.h>

using namespace std::chrono;

namespace Dependencies {

    /**
     * @brief fetch head reference callback 
     * 
     * copies the oid of the reference into the payload to use in the updater
     * @return success [C style bool]
    */
    int FetchHeadRefCallback(const char*, const char*, const git_oid *oid, unsigned int refCallbackIsMerge, void *payload)
    {
        if (refCallbackIsMerge)
        {
            Logger.LogItem("fetchhead_ref_cb", "caught type [merge]");
            git_oid_cpy((git_oid *)payload, oid);
        }
        return 0;
    }

    const int CloneRepository(git_repository* repository, std::string packageLocalPath, std::string remoteObject) 
    {
        git_clone_options gitCloneOpts = GIT_CLONE_OPTIONS_INIT;
        gitCloneOpts.checkout_opts.checkout_strategy = GIT_CHECKOUT_FORCE; 
 
        // can't clone into a non-empty directory
        if (std::filesystem::exists(packageLocalPath) && !std::filesystem::is_empty(packageLocalPath)) 
        {
            std::uintmax_t removedCount = std::filesystem::remove_all(packageLocalPath);
            Logger.LogItem("status", fmt::format("flushed {} items", removedCount));
        }
        else if (!std::filesystem::exists(packageLocalPath))
        {
            try
            {
                std::filesystem::create_directories(packageLocalPath);
            }
            catch (std::filesystem::filesystem_error& ex)
            {
                Logger.Error("failed to create package directories -> {}", ex.what());
            }
        }
        else 
        {
            Logger.LogItem("status", "ready to clone module...");
        }

        Logger.LogItem("status", "cloning modules...");
        int cloneSuccessStatus = git_clone(&repository, remoteObject.c_str(), packageLocalPath.c_str(), &gitCloneOpts);

        if (cloneSuccessStatus != 0) 
        {
            const git_error* lastError = git_error_last();
            const std::string strErrorMessage = fmt::format("Error cloning frontend modules -> {}", lastError->message);

            Logger.LogItem("status", strErrorMessage, true);
            boxer::show(strErrorMessage.c_str(), "Fatal Error", boxer::Style::Error);
        }

        return cloneSuccessStatus;
    }

    const int FetchHead(git_repository* repo, std::string package_path) 
    {
        int gitErrorCode;
        git_remote *remote;

        Logger.LogItem("status", "checking for module updates...");
        gitErrorCode = git_remote_lookup(&remote, repo, "origin");
        
        if (gitErrorCode < 0) 
        {
            Logger.LogItem("error", fmt::format("failed lookup -> {}", git_error_last()->message));
            return 0; 
        }

        git_fetch_options options = GIT_FETCH_OPTIONS_INIT;
        gitErrorCode = git_remote_fetch(remote, NULL, &options, "pull");
        
        if (gitErrorCode < 0) 
        {
            const git_error * last_error = git_error_last();
            Logger.LogItem("error", fmt::format("failed fetch -> -> klass: {}, message: {}", last_error->klass, last_error->message));

            // Couldn't connect to GitHub, and modules dont already exist.
            if (last_error->klass == GIT_ERROR_NET && !std::filesystem::exists(package_path.c_str())) 
            {
                boxer::show("It seems you don't have internet connection or GitHub's API is unreachable. A valid internet connection is required to setup Millennium.", "Error", boxer::Style::Error);
            }
            return 1; 
        }

        git_oid branchOidToMerge;
        git_annotated_commit *annotatedCommit[1];

        git_repository_fetchhead_foreach(repo, FetchHeadRefCallback, &branchOidToMerge);
        gitErrorCode = git_annotated_commit_lookup(&annotatedCommit[0], repo, &branchOidToMerge);

        if (gitErrorCode < 0) 
        {
            Logger.Error("error looking up annotated commit -> {}", git_error_last()->message);
            return 1; 
        }

        git_merge_analysis_t analysisOut;
        git_merge_preference_t preferenceOut;

        Logger.LogItem("status", "analyzing repository information...");
        gitErrorCode = git_merge_analysis(&analysisOut, &preferenceOut, repo, (const git_annotated_commit **) annotatedCommit, 1);

        if (gitErrorCode < 0) 
        {
            Logger.LogItem("error", fmt::format("couldn't analyze -> {}", git_error_last()->message));
            return 1; 
        }

        if (analysisOut & GIT_MERGE_ANALYSIS_UP_TO_DATE) 
        {
            Logger.LogItem("status", "modules seem to be up-to-date!");

            git_annotated_commit_free(annotatedCommit[0]);
            git_repository_state_cleanup(repo);
            git_remote_free(remote);

            return 0;
        } 
        else if (analysisOut & GIT_MERGE_ANALYSIS_FASTFORWARD) 
        {
            Logger.LogItem("status", "fast-forwarding modules...");

            git_reference *referenceOut;
            git_reference *newTargetReference;

            const char* pluginName = "refs/heads/master";

            if (git_reference_lookup(&referenceOut, repo, pluginName) == 0)
            {
                git_reference_set_target(&newTargetReference, referenceOut, &branchOidToMerge, "pull: Fast-forward");
            }

            git_reset_from_annotated(repo, annotatedCommit[0], GIT_RESET_HARD, NULL);

            git_reference_free(referenceOut);
            git_repository_state_cleanup(repo);
        }

        git_annotated_commit_free(annotatedCommit[0]);
        git_repository_state_cleanup(repo);
        git_remote_free(remote);
        return gitErrorCode;
    }

    /**
     * @brief clones and manages index version of millennium modules
     * 
     * clones the __builtin__ modules if they don't exist on disk, and if they do exist
     * they are checked against upstream and pulled if there were commits/changes on the remote. 
     * 
     * @return success status 
    */
    bool GitAuditPackage(std::string common_name, std::string package_path, std::string remote_object)
    {
        const auto startTime = steady_clock::now();
        git_libgit2_init();
        
        Logger.LogHead(fmt::format("libgit - {} [{} ms]", common_name, duration_cast<milliseconds>(steady_clock::now() - startTime).count()));
        Logger.LogItem("modules", package_path);

        git_repository* repo = nullptr;
        int repositoryOpenStatus = git_repository_open(&repo, package_path.c_str());

        switch (repositoryOpenStatus) {
            case GIT_ENOTFOUND: 
            {
                Logger.LogItem("modules", "repo not found...");
                repositoryOpenStatus = CloneRepository(repo, package_path, remote_object);
                break;
            }
            case 0: // no error occured
            {
                Logger.LogItem("modules", "fetching head for updates...");
                repositoryOpenStatus = FetchHead(repo, package_path);
                break;
            } 
            default: {
                const git_error *e = git_error_last();
                Logger.LogItem("error", fmt::format("couldn't evaluate repo -> {}", e->message));
            }
        }

        Logger.LogItem("status", fmt::format("done: [{} ms]; ec: [{}]", duration_cast<milliseconds>(steady_clock::now() - startTime).count(), repositoryOpenStatus), true);
        // Free resources
        git_repository_free(repo);
        git_libgit2_shutdown();
        return repositoryOpenStatus == 0;
    }
}