#!/bin/bash
# not upstream: XR releases built and checked like Orbiter's (packaging/linux/README.md): xr.sh build | package | release [orbiter package]
set -euo pipefail
H=$(dirname "$(readlink -f "$0")"); SRC=$(readlink -f "$H/../.."); cd "$SRC"
ROOT=${ORBITER_RELEASE_ROOT:-$HOME/Projects/orbiter-collision/out/pkg/alma9} # the AlmaLinux 9 folder of Orbiter's rc.sh
LOCAL=${ORBITER_PORTABLE_DIR:-$HOME/Projects/Orbiter-linux-custom/out/portable} # where rc.sh leaves the Orbiter packages
ORBREPO=${ORBITER_RELEASE_REPO:-racerx2/orbiter-2027-linux}
REPO=${XR_RELEASE_REPO:-racerx2/XRVessels}
BRANCH=linux
XR2_DATA=${XR2_DATA:-$HOME/Projects/Orbiter-linux-custom/out/build/linux-x64-release} # an Orbiter folder with Doug's XR2 mesh and textures (tests only)
OUT=$SRC/out/release; W=/work/$(basename "$SRC"); WO=$W/out/release
VESSELS=(DeltaGliderXR1 XR2Ravenstar XR5Vanguard)
GLOBALS=(XRVessels/DeltaGliderXR1/DeltaGliderXR1/XR1Globals.cpp XRVessels/XR2Ravenstar/XR2Ravenstar/XR2Globals.cpp XRVessels/XR5Vanguard/XR5Vanguard/XR5Globals.cpp)
die() { echo "xr: STOP: $*" >&2; exit 1; }
say() { echo "xr: $*" >&2; }

inroot() { # as rc.sh: fake root in the AlmaLinux folder, the repo's parent is /work
	nice -n 10 bwrap --unshare-user --uid 0 --gid 0 --cap-add ALL --bind "$ROOT" / --dev /dev --proc /proc --tmpfs /tmp \
		--ro-bind /etc/resolv.conf /etc/resolv.conf --share-net --bind "$(dirname "$SRC")" /work \
		--setenv HOME /root --setenv PATH /usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin --setenv LANG C.UTF-8 \
		--chdir / "$@"
}

version() { # the modules' VERSION strings; the three must agree
	local v; v=$(sed -n 's/^const char *\* *VERSION = "Version \([^ ]*\) \[.*/\1/p' "${GLOBALS[@]}" | sort -u)
	[ -n "$v" ] && [ "$(wc -l <<< "$v")" = 1 ] || die "the VERSION strings of ${GLOBALS[*]} differ or are missing: $v"
	echo "$v"
}

clean() { [ -z "$(git status --porcelain --untracked-files=no)" ] || die "uncommitted changes"; }

token() { GH_TOKEN=$(printf 'protocol=https\nhost=github.com\n\n' | git credential fill 2>/dev/null | awk -F= '/^password=/{print $2}'); export GH_TOKEN; [ -n "$GH_TOKEN" ] || die "no GitHub token in git's credential store"; }

orbiter() { # $1: a package .tar.xz (with its .sha256) or folder; else the newest local rcN; else the latest release. Sets PKG PKGN PKGSUM, unpacks into out/release/orbiter
	local a=${1:-} t
	[ -n "$a" ] || a=$(ls "$LOCAL"/Orbiter2027-Linux-x86_64-rc*.tar.xz 2> /dev/null | sort -V | tail -1) || true
	if [ -z "$a" ]; then
		t=$(gh release list -R "$ORBREPO" -L 100 --json tagName -q '.[].tagName' | grep -x '2027-rc[0-9]*' | sort -V | tail -1) || die "cannot list the releases of $ORBREPO"
		mkdir -p "$OUT/dl"; a=$OUT/dl/Orbiter2027-Linux-x86_64-${t#2027-}.tar.xz
		say "no local Orbiter package: downloading $t"
		gh release download "$t" -R "$ORBREPO" -D "$OUT/dl" --clobber -p "$(basename "$a")" -p "$(basename "$a").sha256" || die "download of $t"
	fi
	a=$(readlink -f "$a")
	if [ -d "$a" ]; then
		[ -f "$a/Orbitersdk/include/OrbiterAPI.h" ] && [ -f "$a/lib/libQt6Core.so.6" ] || die "$a is not an Orbiter package folder"
		PKG=$a; PKGN=$(basename "$a"); PKGSUM=folder
	else
		[ -f "$a.sha256" ] || die "no $a.sha256"
		(cd "$(dirname "$a")" && sha256sum -c "$(basename "$a").sha256" > /dev/null) || die "sha256 of $a"
		PKG=$a; PKGN=$(basename "$a" .tar.xz); PKGSUM=$(cut -d' ' -f1 "$a.sha256")
	fi
	local O=$OUT/orbiter
	if [ "$a" = "$O/$PKGN" ]; then
		: # already the unpacked copy
	elif [ "$PKGSUM" = folder ] || [ "$(cat "$O/$PKGN.sha256" 2> /dev/null)" != "$PKGSUM" ] || [ ! -d "$O/$PKGN" ]; then
		rm -rf "$O"; mkdir -p "$O"
		unpack "$O"
		echo "$PKGSUM" > "$O/$PKGN.sha256"
	fi
	say "Orbiter package: $PKGN ($PKGSUM)"
}

unpack() { # $1 folder: the Orbiter package as $1/$PKGN
	if [ -d "$PKG" ]; then cp -a "$PKG" "$1/$PKGN"; else tar -xf "$PKG" -C "$1"; fi
	[ -d "$1/$PKGN" ] || die "$PKG has no folder $PKGN"
}

build() { # $1 PACK (0: compile only)
	[ -d "$ROOT/opt/rh/gcc-toolset-14" ] || die "no release root at $ROOT (set ORBITER_RELEASE_ROOT)"
	local qt; qt=$(ls "$ROOT/opt/Qt" | grep -m1 '^6\.') || die "no Qt in $ROOT/opt/Qt"
	inroot env QTDIR=/opt/Qt/$qt/gcc_64 JOBS=$(( $(nproc) > 3 ? $(nproc) - 2 : 1 )) PACK="$1" \
		bash "$W/packaging/linux/build.sh" "$W" "$WO/orbiter/$PKGN" "$WO/build" "$WO/stage" > "$OUT/build.log" 2>&1 \
		|| { tail -15 "$OUT/build.log" >&2; die "release build failed: $OUT/build.log"; }
}

checkso() { # $1.. module files: symbol versions, libraries and imports against the Orbiter package
	local m=() f
	for f in "$@"; do m+=("$W/${f#"$SRC"/}"); done
	inroot python3 "$W/packaging/linux/check.py" "$WO/orbiter/$PKGN" "${m[@]}" || die "module check (packaging/linux/check.py)"
}

FLIGHTS=("DeltaGliderXR1|DG-XR1/In Orbit" "DeltaGliderXR1|DG-XR1/Landed at Brighton Beach"
	"XR5Vanguard|XR5 Vanguard/In Orbit" "XR5Vanguard|XR5 Vanguard/Landed at Brighton Beach"
	"XR2Ravenstar|XR2 Ravenstar/In Orbit, Undocked From ISS" "XR2Ravenstar|XR2 Ravenstar/Landed at Brighton Beach")

flight() { # $1 where (here, root, client), $2 vessel, $3 scenario, $4 frames: in the current folder (the check copy)
	local v=$2 s=$3 tag; tag=$1-$(tr ' ,/' '___' <<< "$s")
	rm -f Orbiter.log "$v.log"
	case $1 in
		here) env -u PIPEWIRE_REMOTE -u PIPEWIRE_RUNTIME_DIR XDG_RUNTIME_DIR="$T/home/run" QT_QPA_PLATFORM=offscreen timeout 120 ./Orbiter "--scenariox=$s" --fixedstep=0.02 "--maxframes=$4" > "$T/$tag.out" 2>&1 || die "$1 flight $s: rc $?, $T/$tag.out";;
		root) local h=$WO/check/home
			inroot env -i HOME="$h" XDG_DATA_HOME="$h/.local/share" XDG_CONFIG_HOME="$h/.config" XDG_CACHE_HOME="$h/.cache" PATH=/usr/bin:/bin LANG=C.UTF-8 QT_QPA_PLATFORM=offscreen \
				bash -c 'cd "$1" && exec timeout 120 ./Orbiter "--scenariox=$2" --fixedstep=0.02 "--maxframes=$3"' flight "$WO/check/$PKGN" "$s" "$4" > "$T/$tag.out" 2>&1 \
				|| die "$1 flight $s: rc $?, $T/$tag.out";;
		client) timeout 120 ./Orbiter "--scenariox=$s" --fixedstep=0.02 "--maxframes=$4" > "$T/$tag.out" 2>&1 || die "Vulkan client flight $s: rc $?, $T/$tag.out";;
	esac
	cp Orbiter.log "$T/$tag.log"; cp "$v.log" "$T/$tag.$v.log" 2> /dev/null || die "$1 flight $s: no $v.log (the module did not run)"
	grep -q "Module $v\.so " Orbiter.log || die "$1 flight $s: $v.so not loaded ($T/$tag.log)"
	! grep -q 'Could not load vessel module' Orbiter.log || die "$1 flight $s: $(grep -m1 'Could not load vessel module' Orbiter.log)"
	grep -F "Version $V [64-bit Release]" "$v.log" | grep -q ' - Loading ' || die "$1 flight $s: not version $V ($T/$tag.$v.log)"
	grep -q 'Using XRSound version' "$v.log" || die "$1 flight $s: no XRSound ($T/$tag.$v.log)"
	! grep -qi 'error\|invalid' "$v.log" || die "$1 flight $s: $(grep -i -m1 'error\|invalid' "$v.log")"
	local e; e=$(grep 'ERROR' Orbiter.log | grep -v 'D3D9ERROR: \(Surface \|UnDeleted Surface\)' | grep -cv 'XRSound .* ERROR: could not initialize default sound device' || true)
	[ "$e" = 0 ] || die "$1 flight $s: $e ERROR lines in $T/$tag.log"
}

check() { # the zips as a user installs them, into a fresh Orbiter package with a temp HOME; flights here, in the AlmaLinux folder and with the Vulkan client
	T=$OUT/check
	local H0=$HOME D=$OUT/$V v z f s
	rm -rf "$T"; mkdir -p "$T/home"; mkdir -m 700 "$T/home/run" # headless flights: no sound server, so they stay silent
	for z in "$D"/*.zip; do (cd "$D" && sha256sum -c "$(basename "$z").sha256" > /dev/null) || die "sha256 of $z"; done
	unpack "$T"
	cd "$T/$PKGN"
	printf 'ACTIVE_MODULES\n  XRSound\nEND_MODULES\n' > Orbiter.cfg # headless flights: no graphics client, XRSound as the XR vessels use it
	for z in "$D"/*.zip; do # a zip never replaces a file of Orbiter's own
		f=$(unzip -Z1 "$z" | while IFS= read -r n; do [ -d "$n" ] || [ ! -e "$n" ] || echo "$n"; done)
		[ -z "$f" ] || die "$(basename "$z") would replace Orbiter's $(head -3 <<< "$f" | paste -sd, -)"
	done
	unzip -q -o "$D/DeltaGliderXR1-$V-x64.zip"
	unzip -q -o "$D/XR5Vanguard-$V-x64.zip"
	if [ -d "$XR2_DATA/Meshes/XR2Ravenstar" ]; then
		python3 "$H/pack.py" xr2 --repo "$SRC" --data "$XR2_DATA" --dest . # what Doug's XR2 package installs
		unzip -q -o "$D/XR2Ravenstar-$V-so-x64.zip"
	else
		say "XR2 flights skipped: no Meshes/XR2Ravenstar in $XR2_DATA (set XR2_DATA)"
	fi
	for v in "${VESSELS[@]}"; do
		[ ! -f "Modules/$v.so" ] || cmp -s "Modules/$v.so" "$OUT/stage/Modules/$v.so" || die "Modules/$v.so from the zip is not the checked module"
	done
	export HOME=$T/home XDG_DATA_HOME=$T/home/.local/share XDG_CONFIG_HOME=$T/home/.config XDG_CACHE_HOME=$T/home/.cache
	local n=0 nc=0 why="" host; host=$(. /etc/os-release 2> /dev/null; echo "${PRETTY_NAME:-this PC}")
	for f in "${FLIGHTS[@]}"; do
		v=${f%%|*}; s=${f#*|}
		[ -f "Modules/$v.so" ] || continue
		flight here "$v" "$s" 200; flight root "$v" "$s" 200; n=$(( n + 2 ))
	done
	say "headless flights: $n (here and in the AlmaLinux 9 folder), each loads its module at $V with no errors"
	if [ "${XR_CLIENT:-1}" = 0 ]; then
		why="XR_CLIENT=0"; say "client flights skipped: $why"
	elif pgrep -x Orbiter > /dev/null; then
		why="Orbiter was running"; say "client flights skipped: $why"
	else
		printf 'StartPaused = FALSE\nFullscreen = FALSE\nWindowWidth = 1280\nWindowHeight = 720\nDeviceIndex = 0\nACTIVE_MODULES\n  VulkanClient\n  XRSound\nEND_MODULES\n' > Orbiter.cfg
		export XDG_RUNTIME_DIR=${XDG_RUNTIME_DIR:-/run/user/$(id -u)} WAYLAND_DISPLAY=${WAYLAND_DISPLAY:-wayland-0} DISPLAY=${DISPLAY:-:0}
		for f in "${FLIGHTS[@]}"; do
			v=${f%%|*}; s=${f#*|}
			[ -f "Modules/$v.so" ] && [ "${s##*/}" != "Landed at Brighton Beach" ] || continue
			flight client "$v" "$s" 150; nc=$(( nc + 1 ))
			say "client flight $s: rc 0, no errors"
		done
	fi
	{
		echo "- The modules against $PKGN: glibc, libstdc++ and Qt symbol versions, libraries, and every Orbiter function they import."
		echo "- The zips installed into a fresh $PKGN with an empty home folder.$([ -f Modules/XR2Ravenstar.so ] || echo ' The XR2 was not flown (no mesh here).')"
		echo "- $n headless flights, two scenarios per vessel, on $host and in AlmaLinux 9 (glibc 2.34): each loads its module at $V with XRSound and no errors."
		if [ "$nc" -gt 0 ]; then echo "- $nc Vulkan client flights, one per vessel, on $host."; else echo "- No Vulkan client flight ($why)."; fi
	} > "$OUT/$V.checks"
	export HOME=$H0; unset XDG_DATA_HOME XDG_CONFIG_HOME XDG_CACHE_HOME
	cd "$SRC"
	say "checks pass: sha256, zips install the checked modules, flights"
}

stamp() { echo "$(git rev-parse HEAD) $(git rev-parse 'HEAD^{tree}') $PKGN $PKGSUM"; }

package() { # $1 Orbiter package (optional)
	clean
	V=$(version)
	say "packaging $V from $(git log -1 --format='%h %s')"
	orbiter "${1:-}"
	rm -f "$OUT/$V.commit"; rm -rf "$OUT/build" "$OUT/stage" "$OUT/$V"
	build 1
	local w; w=$(grep -c 'warning:' "$OUT/build.log" || true)
	[ "$w" = 0 ] || { grep -m5 'warning:' "$OUT/build.log" >&2; die "$w compiler warnings: $OUT/build.log"; }
	say "build: $(grep -m1 '^gcc' "$OUT/build.log"), no warnings"
	local m=() v
	for v in "${VESSELS[@]}"; do m+=("$OUT/stage/Modules/$v.so"); done
	checkso "${m[@]}"
	python3 "$H/pack.py" zips --repo "$SRC" --stage "$OUT/stage" --out "$OUT/$V" --version "$V" --orbiter "$PKGN"
	check
	stamp > "$OUT/$V.commit"
	say "package: $OUT/$V ($(ls "$OUT/$V" | grep -c '\.zip$') zips)"
}

release() { # $1 Orbiter package (optional): package if needed, push linux, publish v<version>
	local TAG C NOTES SUMS z D
	clean
	V=$(version); TAG=v$V; NOTES=$OUT/notes-$V.md
	orbiter "${1:-}"
	[ "$PKGSUM" != folder ] || die "a release is built against a released Orbiter package, not a folder"
	[ "$(cat "$OUT/$V.commit" 2> /dev/null)" = "$(stamp)" ] || package "${1:-}"
	SUMS=$(cd "$OUT/$V" && sha256sum -c ./*.sha256 > /dev/null && cat ./*.sha256) || die "sha256 of the zips"
	local CH=$OUT/changes-$V.md PREV
	if [ ! -s "$CH" ]; then # as rc.sh: without a changes file, the commit subjects since the previous release
		PREV=$({ gh release list -R "$REPO" -L 100 --json tagName -q '.[].tagName'; echo "$TAG"; } | grep -x 'v[0-9.]*-linux-[0-9.]*' | sort -uV | grep -B1 -xF "$TAG" | grep -vxF "$TAG" || true)
		CH=$OUT/changes-$V.auto.md
		if [ -n "$PREV" ] && git fetch -q "https://github.com/$REPO.git" "refs/tags/$PREV" 2> /dev/null; then
			git log --first-parent --format='- %s' FETCH_HEAD..HEAD > "$CH"
		else
			git log --first-parent --format='- %s' -20 > "$CH"
		fi
		say "no $OUT/changes-$V.md: notes list the commits since ${PREV:-(none)}"
	fi
	python3 - "$H/notes.md" "$NOTES" "$V" "$PKGN" "$(git rev-parse --short HEAD)" "$SUMS" "$CH" "$OUT/$V.checks" <<'EOF'
import sys, re
t, o, ver, pkg, commit, sums, ch, checks = sys.argv[1:9]
rc = re.sub(r'^Orbiter2027-Linux-x86_64-', '', pkg).upper()
s = open(t).read().replace('@VER@', ver).replace('@ORBITER@', 'Orbiter 2027 ' + rc).replace('@COMMIT@', commit).replace('@SUMS@', sums.strip())
s = s.replace('@CHANGES@', open(ch).read().strip()).replace('@CHECKS@', open(checks).read().strip())
open(o, 'w').write(s)
EOF
	git fetch -q "https://github.com/$REPO.git" "refs/heads/$BRANCH"
	git merge-base --is-ancestor FETCH_HEAD HEAD || die "$REPO $BRANCH has commits that are not here"
	git push -q "https://github.com/$REPO.git" "HEAD:refs/heads/$BRANCH"
	C=$(git rev-parse HEAD)
	if gh release view "$TAG" -R "$REPO" > /dev/null 2>&1; then # a re-run: same files on a commit with our tree
		git fetch -q "https://github.com/$REPO.git" "refs/tags/$TAG"
		[ "$(git rev-parse 'FETCH_HEAD^{tree}')" = "$(git rev-parse 'HEAD^{tree}')" ] || die "$TAG exists with other files"
		gh release upload "$TAG" -R "$REPO" --clobber "$OUT/$V"/*.zip "$OUT/$V"/*.sha256
	else
		gh release create "$TAG" -R "$REPO" --target "$C" --prerelease --title "XR Vessels $V (unofficial Linux port, x64)" \
			--notes-file "$NOTES" "$OUT/$V"/*.zip "$OUT/$V"/*.sha256 > /dev/null
	fi
	for z in "$OUT/$V"/*.zip; do
		D=$(gh api "repos/$REPO/releases/tags/$TAG" --jq ".assets[] | select(.name == \"$(basename "$z")\") | .digest // \"\"")
		[ -z "$D" ] || [ "$D" = "sha256:$(cut -d' ' -f1 "$z.sha256")" ] || die "GitHub's digest of $(basename "$z") is $D, not our sha256"
	done
	say "https://github.com/$REPO/releases/tag/$TAG (${C:0:8})"
}

case "${1:-}" in
	build) V=$(version); orbiter "${2:-}"; build 0; say "release build compiles (out/release/build)";;
	package) package "${2:-}";;
	release) token; release "${2:-}";;
	*) echo "usage: xr.sh build | package | release [orbiter package]" >&2; exit 2;;
esac
