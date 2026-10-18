#!/bin/sh
# Fail if the app code names a network module or class.
# Usage: check_offline.sh <project source dir>
set -eu

root=$1
pattern='Qt6?::Network|QtNetwork|QNetwork|QTcpSocket|QUdpSocket|QSslSocket|QWebSocket|XMLHttpRequest'

if grep -rnE "$pattern" "$root/CMakeLists.txt" "$root/src" "$root/qml"; then
    echo "FAIL: network code found. The app must stay offline." >&2
    exit 1
fi
echo "OK: no network code."
