#!/usr/bin/env python3
# not upstream: checks XR modules against an Orbiter release package (xr.sh); check.py <orbiter package folder> <module.so>...
import os, re, subprocess, sys

MAXVER = {'GLIBC': (2, 34), 'GLIBCXX': (3, 4, 29), 'CXXABI': (1, 3, 13)}  # Orbiter's collect.py baseline
SYSTEM = ('libc.so.6', 'libm.so.6', 'libstdc++.so.6', 'libgcc_s.so.1', 'ld-linux-x86-64.so.2')
RUNPATH = '$ORIGIN/../lib:$ORIGIN/..'  # what collect.py gives Orbiter's Modules/*.so
WEAK_OK = ('__gmon_start__', '_ITM_registerTMCloneTable', '_ITM_deregisterTMCloneTable')


def readelf(*a):
    r = subprocess.run(['readelf', '-W'] + list(a), capture_output=True, text=True)
    if r.returncode:
        sys.exit('check: readelf %s: %s' % (' '.join(a), r.stderr.strip()))
    return r.stdout


def dynamic(p):
    d = readelf('-d', p)
    return re.findall(r'\(NEEDED\)\s+Shared library: \[([^\]]+)\]', d), re.findall(r'\((RUNPATH|RPATH)\)\s+Library r(?:un)?path: \[([^\]]*)\]', d)


def versions(p):  # (definitions, [(file, version needed)])
    defs, needs, sec, cur = set(), [], None, None
    for l in readelf('-V', p).splitlines():
        if l.startswith('Version definition section'):
            sec = 'd'
        elif l.startswith('Version needs section'):
            sec = 'r'
        elif l.startswith('Version symbols section'):
            sec = None
        elif sec == 'd':
            m = re.search(r'Flags: (\S+).*\bName: (\S+)', l)
            if m and m.group(1) != 'BASE':
                defs.add(m.group(2))
        elif sec == 'r':
            m = re.search(r'\bFile: (\S+)', l)
            if m:
                cur = m.group(1)
            m = re.search(r'\bName: (\S+)', l)
            if m:
                needs.append((cur, m.group(1)))
    return defs, needs


def symbols(p):  # (defined names, [(undefined name, bind)]), names without version
    defined, undef = set(), []
    for l in readelf('--dyn-syms', p).splitlines():
        m = re.match(r'\s*\d+:\s+[0-9a-f]+\s+\S+\s+\S+\s+(\S+)\s+\S+\s+(\S+)\s+(\S+)', l)
        if not m:
            continue
        bind, ndx, name = m.groups()
        base = name.split('@')[0]
        if ndx == 'UND':
            if base:
                undef.append((name, bind))
        elif bind in ('GLOBAL', 'WEAK', 'UNIQUE'):
            defined.add(base)
    return defined, undef


def vkey(v):
    return tuple(int(x) for x in v.split('.'))


def main():
    if len(sys.argv) < 3:
        sys.exit('usage: check.py <orbiter package folder> <module.so>...')
    pkg, mods = sys.argv[1], sys.argv[2:]
    lib = os.path.join(pkg, 'lib')
    prog = os.path.join(pkg, 'Orbiter')
    if not os.path.isfile(prog) or not os.path.isdir(lib):
        sys.exit('check: %s is not an Orbiter package folder' % pkg)
    exports, _ = symbols(prog)
    dl = os.path.join(pkg, 'deps.list')  # the system libraries the launcher makes sure of
    deps = set(re.findall(r'^rpm (\S+)\(\)\(64bit\)$', open(dl).read(), re.M)) if os.path.isfile(dl) else set()
    cache = {}

    def libinfo(n):
        if n not in cache:
            p = os.path.join(lib, n)
            cache[n] = (versions(p)[0], symbols(p)[0]) if os.path.isfile(p) else None
        return cache[n]

    failed = False
    for m in mods:
        bad, name = [], os.path.basename(m)
        needed, paths = dynamic(m)
        for n in needed:
            if n not in SYSTEM and n not in deps and libinfo(n) is None:
                bad.append('needs %s: not in the package lib/, the C and C++ runtime or the package\'s deps.list' % n)
        if paths != [('RUNPATH', RUNPATH)]:
            bad.append('RUNPATH %s, not %s' % (paths, RUNPATH))
        defs, needs = versions(m)
        worst, qt = {}, set()
        for f, v in needs:
            k = re.match(r'(GLIBCXX|GLIBC|CXXABI)_(.*)$', v)
            if k and f in SYSTEM:
                if not re.match(r'^[0-9.]+$', k.group(2)):
                    bad.append('needs %s from %s' % (v, f))
                    continue
                t = vkey(k.group(2))
                worst[k.group(1)] = max(worst.get(k.group(1), ()), t)
                if t > MAXVER[k.group(1)]:
                    bad.append('needs %s (the release baseline is %s_%s)' % (v, k.group(1), '.'.join(map(str, MAXVER[k.group(1)]))))
            elif f in SYSTEM:
                continue
            else:
                li = libinfo(f)
                if li is not None and v not in li[0]:
                    bad.append('needs %s from %s: the package has %s' % (v, f, ', '.join(sorted(x for x in li[0] if x.startswith(v.split('.')[0])))))
                if v.startswith('Qt_6'):
                    qt.add(v)
        _, undef = symbols(m)
        providers = set(exports)
        for n in needed:
            li = libinfo(n)
            if li is not None:
                providers |= li[1]
        own = 0
        for s, bind in undef:
            if '@' in s:
                continue
            if s in providers:
                own += 1
            elif not (bind == 'WEAK' and s in WEAK_OK):
                bad.append('imports %s: not exported by the package\'s Orbiter or its libraries' % s)
        qmax = max(qt, key=lambda v: vkey(v[3:]) if re.match(r'^Qt_[0-9.]+$', v) else (0,), default='none')
        print('check: %s: %s, %s, %d unversioned imports found in the package%s' % (
            name, ', '.join('%s_%s' % (k, '.'.join(map(str, v))) for k, v in sorted(worst.items())), 'Qt up to ' + qmax,
            own, '' if not bad else ': FAILED'))
        for b in sorted(set(bad))[:40]:
            print('  ' + b)
        failed |= bool(bad)
    sys.exit(1 if failed else 0)


main()
