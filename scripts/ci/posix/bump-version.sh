#!/bin/bash

# Get the version from semantic-release dry run
VERSION=$(npx semantic-release --dry-run | grep -oP 'The next release version is \K[0-9.]+')

# Output the version to the console
echo "Version: $VERSION"

# Set the GitHub Actions output variable
echo "::set-output name=version::$VERSION"

# Write the version to the file
echo "# current version of millennium" > ./version
echo "v$VERSION" >> ./version
