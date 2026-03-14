#!/bin/bash
#-------------------------------------------------------------------------------------------------
#
#  release.sh
#  cmacs
#
#  Copyright 2022-2026 Todd Vernon. All rights reserved.
#  Licensed under the Apache License, Version 2.0
#  See LICENSE file for details.
#
#  Release script - builds, packages, and publishes a cmacs release.
#
#  Usage: ./release.sh 1.3
#
#  Steps:
#    1. Updates CmVersion.h
#    2. Pauses for Linux build (Dropbox sync)
#    3. Builds macOS binary
#    4. Creates platform tarballs
#    5. Commits, tags, pushes
#    6. Creates GitHub release with assets
#
#-------------------------------------------------------------------------------------------------

set -e

if [ -z "$1" ]; then
    echo "Usage: ./release.sh <version>"
    echo "Example: ./release.sh 1.3"
    exit 1
fi

VERSION=$1
STAGING="/tmp/cmacs-release-$$"

echo "=== cmacs release v$VERSION ==="
echo ""

#-------------------------------------------------------------------------------------------------
# 1. Update CmVersion.h
#-------------------------------------------------------------------------------------------------

cat > CmVersion.h << EOF
//-------------------------------------------------------------------------------------------------
//
//  CmVersion.h
//  cmacs
//
//  Copyright 2022-2026 Todd Vernon. All rights reserved.
//  Licensed under the Apache License, Version 2.0
//  See LICENSE file for details.
//
//  Version string for cmacs. Updated by release.sh script.
//
//-------------------------------------------------------------------------------------------------

#ifndef _CmVersion_h_
#define _CmVersion_h_

#define CM_VERSION "$VERSION"

#endif
EOF

echo "Updated CmVersion.h to $VERSION"
echo ""

#-------------------------------------------------------------------------------------------------
# 2. Pause for Linux build
#-------------------------------------------------------------------------------------------------

echo "CmVersion.h has been updated. Build on Linux now:"
echo "  cd ~/dev/cx/cx_apps/cm && make clean && make"
echo ""
echo "Press Enter when the Linux build is complete..."
read -r

#-------------------------------------------------------------------------------------------------
# 3. Build macOS
#-------------------------------------------------------------------------------------------------

echo "Building macOS..."
make clean && make
echo ""

#-------------------------------------------------------------------------------------------------
# 4. Verify binaries
#-------------------------------------------------------------------------------------------------

if [ ! -f darwin_arm64/cm ]; then
    echo "Error: darwin_arm64/cm not found"
    exit 1
fi

if [ ! -f linux_x86_64/cm ]; then
    echo "Warning: linux_x86_64/cm not found - Linux tarball will be skipped"
fi

#-------------------------------------------------------------------------------------------------
# 5. Create tarballs
#-------------------------------------------------------------------------------------------------

rm -rf "$STAGING"

# macOS tarball (version-less name so releases/latest/download URLs always work)
MACDIR="$STAGING/cmacs-macos"
mkdir -p "$MACDIR"
cp darwin_arm64/cm "$MACDIR/cm"
cp install.sh "$MACDIR/install.sh"
cp cm_help.md "$MACDIR/cm_help.md"
cp cm_help.txt "$MACDIR/cm_help.txt"
tar czf "cmacs-macos.tar.gz" -C "$STAGING" "cmacs-macos"
echo "Created cmacs-macos.tar.gz"

# Linux tarball
if [ -f linux_x86_64/cm ]; then
    LINDIR="$STAGING/cmacs-linux"
    mkdir -p "$LINDIR"
    cp linux_x86_64/cm "$LINDIR/cm"
    cp install.sh "$LINDIR/install.sh"
    cp cm_help.md "$LINDIR/cm_help.md"
    cp cm_help.txt "$LINDIR/cm_help.txt"
    tar czf "cmacs-linux.tar.gz" -C "$STAGING" "cmacs-linux"
    echo "Created cmacs-linux.tar.gz"
fi

rm -rf "$STAGING"
echo ""

#-------------------------------------------------------------------------------------------------
# 6. Commit, tag, push
#-------------------------------------------------------------------------------------------------

git add CmVersion.h
git commit -m "Release v$VERSION"
git tag "v$VERSION"
git push
git push --tags
echo ""

#-------------------------------------------------------------------------------------------------
# 7. Create GitHub release
#-------------------------------------------------------------------------------------------------

ASSETS="cmacs-macos.tar.gz"
if [ -f "cmacs-linux.tar.gz" ]; then
    ASSETS="$ASSETS cmacs-linux.tar.gz"
fi

RELEASE_BODY="$(cat <<EOF
## cmacs $VERSION

Pre-built binaries for macOS and Linux.

### Install

\`\`\`bash
# Download and extract
tar xzf cmacs-macos.tar.gz   # or cmacs-linux.tar.gz
cd cmacs-macos

# Install to /usr/local
./install.sh
\`\`\`

This installs:
- \`cm\` binary to \`/usr/local/bin/\`
- Help files to \`/usr/local/share/cm/\`

### Build from source

See the [README](https://github.com/toddvernon/cm#getting-started) for build instructions.
EOF
)"

gh release create "v$VERSION" \
    --title "cmacs $VERSION" \
    --notes "$RELEASE_BODY" \
    $ASSETS

echo ""
echo "=== Release v$VERSION published ==="
echo "https://github.com/toddvernon/cm/releases/tag/v$VERSION"

# Clean up tarballs from working directory
rm -f "cmacs-macos.tar.gz" "cmacs-linux.tar.gz"
