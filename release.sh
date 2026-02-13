#!/bin/bash
#-------------------------------------------------------------------------------------------------
#
#  release.sh
#  cmacs
#
#  Copyright 2022-2025 Todd Vernon. All rights reserved.
#  Licensed under the Apache License, Version 2.0
#  See LICENSE file for details.
#
#  Release script - updates version, commits, and tags in one step.
#  Usage: ./release.sh 1.0.1
#
#-------------------------------------------------------------------------------------------------

if [ -z "$1" ]; then
    echo "Usage: ./release.sh <version>"
    echo "Example: ./release.sh 1.0.1"
    exit 1
fi

VERSION=$1

# Update CmVersion.h
cat > CmVersion.h << EOF
//-------------------------------------------------------------------------------------------------
//
//  CmVersion.h
//  cmacs
//
//  Copyright 2022-2025 Todd Vernon. All rights reserved.
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

# Stage, commit, and tag
git add CmVersion.h
git commit -m "Release v$VERSION"
git tag "v$VERSION"

echo "Committed and tagged v$VERSION"
