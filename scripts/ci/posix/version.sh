VERSION=$(npx semantic-release --dry-run | grep -oP 'The next release version is \K[0-9.]+')
echo "Version: $VERSION"
echo "version=$VERSION" >> "$GITHUB_OUTPUT"
echo "# current version of millennium" > ./version
echo "v$VERSION" >> ./version
