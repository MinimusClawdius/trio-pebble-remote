#!/bin/bash
# Build script for Trio Pebble Remote App
# Uses local Pebble SDK at /home/jt/.pebble-sdk

set -e

cd /home/jt/.openclaw/workspace/trio-pebble/remote-app

echo "=== Building Trio Pebble Remote ==="
echo "SDK: /home/jt/.pebble-sdk/SDKs/4.3"

# Check if SDK exists
if [ ! -d "/home/jt/.pebble-sdk/SDKs/4.3" ]; then
    echo "Error: SDK not found at /home/jt/.pebble-sdk/SDKs/4.3"
    exit 1
fi

# Build using waf
/home/jt/.pebble-sdk/SDKs/4.3/sdk-core/pebble/waf configure
/home/jt/.pebble-sdk/SDKs/4.3/sdk-core/pebble/waf build

# Create zip for installation
cd build
zip -r ../trio-pebble-remote.zip .
cd ..

echo ""
echo "=== Build Complete ==="
echo "Output: trio-pebble-remote.zip"
echo ""
echo "To install:"
echo "  pebble install --phone <your-phone-ip>"
echo ""
echo "Or use Rebble app on your phone"
