VERSION=$(npx semantic-release --dry-run | grep -oP 'The next release version is \K[0-9.]+')
echo "Version: $VERSION"
echo "::set-output name=version::$VERSION"
echo "# current version of millennium" > ./version
echo "v$VERSION" >> ./version

git config user.email "github-actions@github.com"
git config user.name "GitHub Actions"
git add version
git commit -m "chore(release): bump version to v$VERSION"
git push