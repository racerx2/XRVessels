#!/usr/bin/env python3
# not upstream: the XR release zips with the file lists of Doug's dist/makedist*.bat (xr.sh)
# pack.py zips --repo R --stage S --out O --version V --orbiter N | pack.py xr2 --repo R --data D --dest T
import argparse, fnmatch, hashlib, os, re, shutil, subprocess, sys, time, zipfile

HERE = os.path.dirname(os.path.abspath(__file__))


def ci(path):  # on-disk spelling of a path given in any letter case (the .bat files ran on Windows)
    parts, cur = path.split('/'), '/'
    for p in parts[1:]:
        hit = [f for f in os.listdir(cur) if f.lower() == p.lower()]
        if not hit:
            raise FileNotFoundError(path)
        cur = os.path.join(cur, hit[0])
    return cur


def files(pattern, recursive=False):
    d, pat = os.path.split(pattern)
    d = ci(d)
    out = sorted(os.path.join(r, f) for r, _, fs in (os.walk(d) if recursive else [(d, None, os.listdir(d))]) for f in fs
                 if os.path.isfile(os.path.join(r, f)) and fnmatch.fnmatch(f.lower(), pat.lower()))
    if not out:
        raise FileNotFoundError(pattern)
    return out


def lists(repo, xr2data=None):  # (vessel, short name, [(zip folder, files)], folders the .bat creates)
    O, X = repo + '/Orbiter', repo + '/XRVessels'
    xr1 = ('DeltaGliderXR1', 'XR1', [
        ('Doc', [ci(X + '/XR Flight Operations Manual.pdf')]),
        ('Config', [ci(X + '/DeltaGliderXR1/DeltaGliderXR1Prefs.cfg')]),
        ('Images/Vessels', [ci(O + '/images/vessels/DeltagliderXR1.bmp')]),
        ('Config', [ci(O + '/config/XR1-EXAMPLE.xrcfg')]),
        ('Config/Vessels', [ci(O + '/config/vessels/DeltagliderXR1.cfg')]),
        ('Meshes/DG-XR1', files(O + '/meshes/DG-XR1/*.msh')),
        ('Scenarios/DG-XR1', files(O + '/scenarios/DG-XR1/*.scn')),
        ('Textures/DG-XR1', files(O + '/textures/DG-XR1/*.dds')),
    ], ['Images/Vessels', 'Config/Vessels', 'Doc', 'Meshes/DG-XR1', 'Modules', 'Scenarios/DG-XR1', 'Textures/DG-XR1'])
    xr5 = ('XR5Vanguard', 'XR5', [
        ('Doc', [ci(X + '/XR Flight Operations Manual.pdf')]),
        ('Config', [ci(X + '/XR5Vanguard/XR5VanguardPrefs.cfg')]),
        ('Doc', [ci(O + '/Doc/XR5-ER Extended Range Payload Systems 1.txt')]),
        ('Config', [ci(O + '/config/XR5-EXAMPLE.xrcfg')]),
        ('Config/Vessels', [ci(O + '/config/vessels/' + n) for n in ['XR5Vanguard.cfg', 'XRPayloadBay.cfg', 'XRParts.cfg', 'XRPayloadTest.cfg',
            'XRPayload_sc_40.cfg', 'AIA_Logistics.cfg', 'XR1_Spare_Parts_Thumbnail.bmp', 'Altea_Default_Payload_Thumbnail.bmp', 'AIAlogistics.bmp']]),
        ('Config/Vessels', files(O + '/config/vessels/CSA_XR5_ER_*.cfg') + files(O + '/config/vessels/CSA_XR5_ER_*.bmp')),
        ('Meshes/XR5Vanguard', files(O + '/meshes/XR5Vanguard/*.msh')),
        ('Meshes', [ci(O + '/meshes/XRPayloadBay.msh')]),
        ('Meshes/XRPayload', files(O + '/meshes/XRPayload/*.msh')),
        ('Scenarios/XR5 Vanguard', files(O + '/scenarios/XR5 Vanguard/*.scn')),
        ('Textures/XR5Vanguard', files(O + '/textures/XR5Vanguard/*.dds')),
        ('Textures/XRPayload', files(O + '/textures/XRPayload/*.dds')),
    ], ['Images/Vessels', 'Config/Vessels', 'Doc', 'Meshes/XR5Vanguard', 'Meshes/XRPayload', 'Modules', 'Scenarios/XR5 Vanguard', 'Textures/XR5Vanguard', 'Textures/XRPayload'])
    if xr2data is None:
        return xr1, xr5
    xr2 = ('XR2Ravenstar', 'XR2', [  # makedistXR2.bat: Doug's package, with the mesh and textures from xr2data
        ('Doc', [ci(X + '/XR Flight Operations Manual.pdf')]),
        ('Config', [ci(X + '/XR2Ravenstar/XR2RavenstarPrefs.cfg')]),
        ('Images/Vessels', [ci(O + '/images/vessels/XR2Ravenstar.bmp')]),
        ('Config', [ci(O + '/config/' + n) for n in ['XR2-EXAMPLE.xrcfg', 'XR2-PhobosDeimosPayloadMission.xrcfg']]),
        ('Config/Vessels', [ci(O + '/config/vessels/' + n) for n in ['XR2Ravenstar.cfg', 'XRPayloadBay.cfg', 'XR2turbopackLee.cfg', 'XR2turbopackKara.cfg',
            'XR2PayloadCHM.cfg', 'XR2PayloadLOX.cfg', 'XR2PayloadLOX_Half.cfg', 'XR2PayloadEmptyLOX.cfg', 'XR2PayloadMainFuel.cfg', 'XR2PayloadSCRAMFuel.cfg',
            'PayloadThumbnailLOX.bmp', 'PayloadThumbnailMainFuel.bmp', 'PayloadThumbnailSCRAMFuel.bmp', 'PayloadThumbnailLOX_Half.bmp',
            'PayloadThumbnailCHM.bmp', 'Altea_Default_Payload_Thumbnail.bmp']]),
        ('Meshes', [ci(O + '/meshes/XRPayloadBay.msh')]),
        ('Meshes/XR2Ravenstar', files(xr2data + '/meshes/XR2Ravenstar/*.msh')),
    ] + [('Textures/XR2Ravenstar/' + os.path.relpath(os.path.dirname(f), ci(xr2data + '/textures/XR2Ravenstar')), [f])
         for f in files(xr2data + '/textures/XR2Ravenstar/*.dds', True)]
      + [('Scenarios/XR2 Ravenstar/' + os.path.relpath(os.path.dirname(f), ci(O + '/scenarios/XR2 Ravenstar')), [f])
         for f in files(O + '/scenarios/XR2 Ravenstar/*.scn', True)], [])
    return (xr2,)


def entries(groups):
    out = []
    for folder, srcs in groups:
        for s in srcs:
            out.append((os.path.normpath(folder + '/' + os.path.basename(s)), s))
    return out


def readme(vessel, orbiter, commit, body):
    rc = re.sub(r'^Orbiter2027-Linux-x86_64-', '', orbiter).upper()
    return open(os.path.join(HERE, 'notice.txt')).read() + body + f'''
Needs Orbiter 2027 {rc} for Linux or newer (github.com/racerx2/orbiter-2027-linux). Built against that release's SDK
the way the release itself is built (AlmaLinux 9, GCC 14, Qt 6.8), so it runs on every distribution that Orbiter runs on.
Source: https://github.com/racerx2/XRVessels (commit {commit}), the Linux port of https://github.com/dbeachy1/XRVessels (version 2.0),
licensed under the GNU GPL v3{' (code, meshes and textures)' if vessel != 'XR2Ravenstar' else ''}.
''' + ('Manual: Doc/XR Flight Operations Manual.pdf\n' if vessel != 'XR2Ravenstar' else '') + open(os.path.join(HERE, 'credits.txt')).read()


def full_body(vessel, short, ver):
    return f'''{short} ({vessel}) {ver} (unofficial Linux port, 64-bit)

Unzip into your Linux Orbiter folder (the one with the Orbiter program, Modules/ and Config/), keeping the folder structure.
Same files as Doug Beachy's {short} 2.0 package (dist/makedist{short}.bat), with the Windows module Modules/{vessel}.dll
replaced by the Linux module Modules/{vessel}.so.
'''


XR2_BODY = '''XR2 Ravenstar {ver} module (unofficial Linux port, 64-bit)

This zip holds only the Linux module Modules/XR2Ravenstar.so. The XR2's mesh and textures are copyright Steve Tyler (Coolhand)
and are distributed only by Doug Beachy, so they are not included.

Install:
1. Install Doug Beachy's official XR2 Ravenstar 2.0 package (XR2Ravenstar-2.0-x86.zip or -x64.zip) into your Linux Orbiter folder.
2. Unzip this zip into the same folder, keeping the folder structure. Linux Orbiter loads Modules/XR2Ravenstar.so;
   the Windows Modules/XR2Ravenstar.dll from Doug's package is ignored and can stay.
'''


def write_zip(dst, items, dirs, when):  # items: (name, source path or bytes)
    names = [n for n, _ in items]
    if len(names) != len(set(names)):
        sys.exit('pack: duplicate entries in ' + os.path.basename(dst))
    alldirs = sorted({'/'.join(n.split('/')[:i]) for n in names for i in range(1, n.count('/') + 1)}
                     | {'/'.join(d.split('/')[:i]) for d in dirs for i in range(1, d.count('/') + 2)})
    with zipfile.ZipFile(dst + '.part', 'w', zipfile.ZIP_DEFLATED, compresslevel=9) as z:
        for d in alldirs:
            zi = zipfile.ZipInfo(d + '/', date_time=when)
            zi.external_attr = (0o40755 << 16) | 0x10
            z.writestr(zi, b'')
        for name, src in items:
            zi = zipfile.ZipInfo(name, date_time=when)
            zi.external_attr = (0o100755 if name.endswith('.so') else 0o100644) << 16
            zi.compress_type = zipfile.ZIP_DEFLATED
            z.writestr(zi, src if isinstance(src, bytes) else open(src, 'rb').read())
    os.replace(dst + '.part', dst)
    with zipfile.ZipFile(dst) as z:
        bad = z.testzip()
        if bad:
            sys.exit('pack: CRC error in %s: %s' % (dst, bad))
        for name, src in items:
            if z.read(name) != (src if isinstance(src, bytes) else open(src, 'rb').read()):
                sys.exit('pack: %s in %s differs from its source' % (name, dst))
        n = len(z.namelist())
    h = hashlib.sha256(open(dst, 'rb').read()).hexdigest()
    with open(dst + '.sha256', 'w') as f:
        f.write('%s  %s\n' % (h, os.path.basename(dst)))
    print('pack: %s: %d entries, %d bytes, %s' % (os.path.basename(dst), n, os.path.getsize(dst), h))


def zips(a):
    repo = os.path.abspath(a.repo)
    commit = subprocess.run(['git', '-C', repo, 'rev-parse', '--short', 'HEAD'], capture_output=True, text=True, check=True).stdout.strip()
    ct = int(subprocess.run(['git', '-C', repo, 'log', '-1', '--format=%ct'], capture_output=True, text=True, check=True).stdout)
    when = time.gmtime(ct)[:6]  # the commit's time on every entry: the same commit gives the same zips
    lic = os.path.join(repo, 'LICENSE')  # GPL v3 travels with the binaries
    os.makedirs(a.out, exist_ok=True)
    for vessel, short, groups, dirs in lists(repo):
        txt = readme(vessel, a.orbiter, commit, full_body(vessel, short, a.version)).encode()
        items = entries(groups) + [('Modules/%s.so' % vessel, os.path.join(a.stage, 'Modules', vessel + '.so')),
                                   ('Doc/%s-Linux.txt' % vessel, txt), ('Doc/XRVessels-LICENSE.txt', lic)]
        write_zip(os.path.join(a.out, '%s-%s-x64.zip' % (vessel, a.version)), items, dirs, when)
    txt = readme('XR2Ravenstar', a.orbiter, commit, XR2_BODY.format(ver=a.version)).encode()
    write_zip(os.path.join(a.out, 'XR2Ravenstar-%s-so-x64.zip' % a.version),
              [('Doc/XR2Ravenstar-Linux.txt', txt), ('Doc/XRVessels-LICENSE.txt', lic),
               ('Modules/XR2Ravenstar.so', os.path.join(a.stage, 'Modules', 'XR2Ravenstar.so'))], ['Doc', 'Modules'], when)


def xr2(a):  # tests only: installs what Doug's XR2 package holds (the cfgs from the repo, the mesh and textures from --data)
    (vessel, short, groups, _), = lists(os.path.abspath(a.repo), os.path.abspath(a.data))
    n = 0
    for name, src in entries(groups):
        d = os.path.join(a.dest, name)
        os.makedirs(os.path.dirname(d), exist_ok=True)
        shutil.copyfile(src, d)
        n += 1
    print('pack: XR2 data for the tests: %d files from the repo and %s' % (n, a.data))


ap = argparse.ArgumentParser()
sub = ap.add_subparsers(dest='cmd', required=True)
p = sub.add_parser('zips')
for k in ('repo', 'stage', 'out', 'version', 'orbiter'):
    p.add_argument('--' + k, required=True)
p = sub.add_parser('xr2')
for k in ('repo', 'data', 'dest'):
    p.add_argument('--' + k, required=True)
a = ap.parse_args()
zips(a) if a.cmd == 'zips' else xr2(a)
