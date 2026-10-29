#!/bin/bash
# not upstream: builds the XR modules inside the AlmaLinux 9 folder of Orbiter's release (xr.sh); build.sh <src> <orbiter package folder> <build> <stage>
set -euo pipefail
SRC=$(readlink -f "$1"); ORB=$(readlink -f "$2"); B=$3; STAGE=$4
source /opt/rh/gcc-toolset-14/enable
mkdir -p "$B"
cmake -S "$SRC" -B "$B" -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="$QTDIR" \
	-DORBITER_ROOT="$ORB" -DORBITER_CMAKE_DIR="$ORB/cmake" -DCMAKE_INSTALL_PREFIX="$B/unused-install"
cmake --build "$B" -j "${JOBS:-$(nproc)}"
[ "${PACK:-1}" = 1 ] || exit 0 # PACK=0: compile only (xr.sh build)
rm -rf "$STAGE"; mkdir -p "$STAGE/Modules"
for m in DeltaGliderXR1 XR2Ravenstar XR5Vanguard; do
	install -m 755 "$B/$m.so" "$STAGE/Modules/$m.so"
	patchelf --set-rpath '$ORIGIN/../lib:$ORIGIN/..' "$STAGE/Modules/$m.so" # as collect.py gives Orbiter's own modules
done
gcc --version | head -1
