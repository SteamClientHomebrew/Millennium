#pragma once
#include <string>

namespace Dependencies 
{
    bool GitAuditPackage(std::string commonName, std::string packagePath, std::string remoteObject);
}

namespace DistributedGit
{
    const bool CloneRepository(std::string repositoryPath, std::string gitRemoteUrl);
    const bool UpdateRepository(std::string packageLocalPath);
    const bool IsRepository(std::string packageLocalPath);
    const std::string GetLocalCommit(std::string packageLocalPath);
}