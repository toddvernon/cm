#!/bin/sh
#
# cmacs installer
# Installs cm binary and help files
#

PREFIX="${PREFIX:-/usr/local}"
BIN_DIR="$PREFIX/bin"
SHARE_DIR="$PREFIX/share/cm"

echo "Installing cmacs to $PREFIX..."

# Install binary
if [ -f cm ]; then
    sudo cp cm "$BIN_DIR/cm"
    sudo chmod 755 "$BIN_DIR/cm"
    # Clear quarantine on macOS
    if [ "$(uname -s)" = "Darwin" ]; then
        sudo xattr -cr "$BIN_DIR/cm" 2>/dev/null
    fi
    echo "  $BIN_DIR/cm"
else
    echo "Error: cm binary not found"
    exit 1
fi

# Install help files
sudo mkdir -p "$SHARE_DIR"
if [ -f cm_help.md ]; then
    sudo cp cm_help.md "$SHARE_DIR/cm_help.md"
    sudo chmod 644 "$SHARE_DIR/cm_help.md"
    echo "  $SHARE_DIR/cm_help.md"
fi
if [ -f cm_help.txt ]; then
    sudo cp cm_help.txt "$SHARE_DIR/cm_help.txt"
    sudo chmod 644 "$SHARE_DIR/cm_help.txt"
    echo "  $SHARE_DIR/cm_help.txt"
fi

echo "Done. Run 'cm' to start editing."
