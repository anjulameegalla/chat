#!/bin/bash
# Chat client launcher for Kali / Debian-based Linux.
# Usage:  bash run.sh            (connects to the default server below)
#         bash run.sh <ip>       (connects to a different server IP)

SERVER_IP="${1:-20.18.59.80}"

# Always work from the folder this script lives in.
cd "$(dirname "$0")" || exit 1

if [ ! -f client.c ]; then
  echo "client.c not found. Put it in the same folder as run.sh."
  exit 1
fi

if ! command -v gcc >/dev/null 2>&1; then
  echo "gcc not found. Install it with:"
  echo "  sudo apt install build-essential"
  exit 1
fi

echo "Compiling client..."
if ! gcc -pthread -o client client.c; then
  echo "Compile failed."
  exit 1
fi

echo "Connecting to $SERVER_IP ..."
echo "(After entering your username, just type a message and press Enter.)"
./client "$SERVER_IP"
