#!/usr/bin/env python3
"""Run lba2cc on an Xbox in Developer Mode as if it were the local binary.

Point LBA2_BIN at this and tests/automation runs against the console, like
scripts/dev/android-lba2cc.sh does for a phone:

    export LBA2_BIN=$PWD/tools/uwp/xbox-lba2cc.py
    export LBA2_GAME_DIR=<the data the console holds in LocalState\\Game>
    bash tests/automation/test_cli_contract.sh

Each call is one run of the app. The arguments go to LocalState\\args.txt and
the environment to LocalState\\env.txt, whose presence makes the app run
scripted (ENTRY.CPP): stdout, stderr and the exit code come back from
stdout.txt, stderr.txt and exit.txt (stdout.txt alone, in order, when both
streams go to one file here). Runs are one at a time: the console runs
one copy of an app.

Paths. Fixtures pass host paths and read results back from them, so every
occurrence of a host root in an argument or a variable is rewritten:

    temp roots (TMPDIR, the macOS user temp folder, /tmp) -> LocalState\\t\\run\\<n>
    LBA2_GAME_DIR                                          -> LocalState\\Game
    the save fixture's folder                              -> LocalState\\t\\save
    the repository                                         -> LocalState\\t\\repo

A named temp path that exists is copied up before the run, and its folder is
copied back after it, so a log or lba2.cfg next to it comes back too. The game
data is NOT copied: LBA2_GAME_DIR must hold what the console holds (checked by
LBA2.HQR's size). Paths the engine prints are mapped back.

Files the run deletes are deleted here too, when this wrapper put them here.
Not supported: stdin, arguments with newlines, more than 127 arguments of up to 1023 bytes.

Env:
  LBA2_XBOX_EXE   the lba2cc.exe to run (default out/build/uwp); it replaces
                  the console's copy when it differs from the last one sent
  XBOX_SH         the Device Portal helper (default tools/uwp/xbox-portal.sh,
                  which reads XBOX_HOST and the login; see its header)
  LBA2_XBOX_STATE state shared between calls (default /tmp/lba2cc-xbox-state)
"""

import hashlib
import json
import os
import re
import signal
import subprocess
import sys
import time
import urllib.parse

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(HERE))
XBOX = os.environ.get("XBOX_SH", os.path.join(HERE, "xbox-portal.sh"))
EXE = os.environ.get("LBA2_XBOX_EXE", os.path.join(REPO, "out/build/uwp/SOURCES/lba2cc.exe"))
STATE = os.environ.get("LBA2_XBOX_STATE", "/tmp/lba2cc-xbox-state")
MAX_ARGS, ARG_SIZE = 127, 1023
# Variables the engine reads, passed through env.txt.
ENV_VARS = ("LBA2_USER_DIR LBA2_GAME_DIR LBA2_LOG_LEVEL LBA2_PROFILE LBA2_REC_HASH_EVERY "
            "LBA2_REC_HASH32 LBA2_SAVE_LOAD_ABI LBA2_TEXFILTER LBA2_PRESENT_FILTER LBA2_ISO_DIV "
            "LBA2_DBG_ISOLATE NO_COLOR").split()
START_WAIT, POLL = 30.0, 0.5


def note(msg):
    sys.stderr.write("xbox-lba2cc: %s\n" % msg)


def die(msg, rc=125):
    note(msg)
    sys.exit(rc)


def q(s):
    return urllib.parse.quote(s, safe="")


PACKAGE = subprocess.run([sys.executable, "-I", os.path.join(HERE, "package-name.py")],
                         capture_output=True, text=True, check=True).stdout.strip()
LOCAL_Q = "knownfolderid=LocalAppData&packagefullname=" + q(PACKAGE)


def api(method, path, *curl, check=True, binary=False):
    r = subprocess.run([XBOX, "api", method, path, *curl], capture_output=True)
    if check and r.returncode != 0:
        die("portal %s %s failed: %s" % (method, path.split("?")[0], r.stderr.decode(errors="replace").strip()))
    return r.stdout if binary else r.stdout.decode(errors="replace")


def ls(dev_path):
    """Items of a LocalState folder ('\\LocalState\\t') as (name, is_folder), or None if it is missing."""
    items = ls_sizes(dev_path)
    return None if items is None else [(name, size is None) for name, size in items.items()]


def ls_sizes(dev_path):
    """{name: size or None for a folder} of a LocalState folder, or None if it is missing."""
    r = subprocess.run([XBOX, "api", "GET", "/api/filesystem/apps/files?%s&path=%s" % (LOCAL_Q, q(dev_path))],
                       capture_output=True)
    if r.returncode != 0:
        return None
    return {i["Name"]: (None if i.get("Type", 0) & 16 else i.get("FileSize"))
            for i in json.loads(r.stdout or b"{}").get("Items", [])}


# Host files this run uploaded or brought back: the ones a deletion on the
# console may be mirrored onto.
TOUCHED = set()


def own_output(path):
    """Whether path is where this wrapper's stdout or stderr goes. A fixture
    may send them into a folder the run brings back; the console's copy of
    that file is empty, and copying it over would erase what was printed."""
    try:
        st = os.stat(path)
    except OSError:
        return False
    for fd in (1, 2):
        try:
            o = os.fstat(fd)
        except OSError:
            continue
        if (o.st_dev, o.st_ino) == (st.st_dev, st.st_ino):
            return True
    return False


def sync_live(dev, host, sizes):
    """While the app runs: files whose size changed come back, so a fixture
    watching a log sees it grow (some stop the run with SIGKILL once a line is in)."""
    items = ls_sizes(dev)
    if items is None:
        return
    os.makedirs(host, exist_ok=True)
    for name, size in items.items():
        key = dev + "\\" + name
        if size is None:
            sync_live(key, os.path.join(host, name), sizes)
            continue
        if sizes.get(key) == size or own_output(os.path.join(host, name)):
            continue
        data = download(dev, name)
        if data is None:
            continue
        with open(os.path.join(host, name), "wb") as f:
            f.write(data)
        TOUCHED.add(os.path.join(host, name))
        sizes[key] = size


def mkdirs(dev_path):
    parent = ""
    for part in [p for p in dev_path.split("\\") if p]:
        items = ls(parent or "\\")
        if items is None or (part, True) not in items:
            api("POST", "/api/filesystem/apps/folder?%s&path=%s&newfoldername=%s" % (LOCAL_Q, q(parent or "\\"), q(part)),
                "-d", "", "-o", "/dev/null", check=False)
        parent += "\\" + part


def upload(dev_dir, files):
    if files:
        args = []
        for n, f in enumerate(files):
            args += ["-F", "f%d=@%s" % (n, f)]
        api("POST", "/api/filesystem/apps/file?%s&path=%s" % (LOCAL_Q, q(dev_dir)), *args, "-o", "/dev/null")


def download(dev_dir, name):
    # Through a file: `api` prints a newline after the body.
    os.makedirs(STATE, exist_ok=True)
    tmp = os.path.join(STATE, "download.part")
    r = subprocess.run([XBOX, "api", "GET", "/api/filesystem/apps/file?%s&path=%s&filename=%s" % (LOCAL_Q, q(dev_dir), q(name)),
                        "-o", tmp], capture_output=True)
    try:
        with open(tmp, "rb") as f:
            data = f.read()
        os.remove(tmp)
    except OSError:
        return None
    return data if r.returncode == 0 else None


def remove(dev_dir, name):
    api("DELETE", "/api/filesystem/apps/file?%s&path=%s&filename=%s" % (LOCAL_Q, q(dev_dir), q(name)), check=False)


def remove_tree(dev_path):
    items = ls(dev_path)
    if items is None:
        return
    for name, is_dir in items:
        if is_dir:
            remove_tree(dev_path + "\\" + name)
        else:
            remove(dev_path, name)
    parent, _, name = dev_path.rpartition("\\")
    api("DELETE", "/api/filesystem/apps/folder?%s&path=%s&filename=%s" % (LOCAL_Q, q(parent or "\\"), q(name)), check=False)


def upload_tree(host, dev):
    """A host file or folder to LocalState path dev (the file's own path, or the folder's)."""
    if os.path.isfile(host):
        parent = dev.rpartition("\\")[0]
        mkdirs(parent)
        # The portal names the file after the upload's file name.
        if os.path.basename(host) != dev.rpartition("\\")[2]:
            die("cannot upload %s under another name" % host)
        upload(parent, [host])
        TOUCHED.add(host)
        return
    for root, dirs, files in os.walk(host):
        rel = os.path.relpath(root, host)
        target = dev if rel == "." else dev + "\\" + rel.replace("/", "\\")
        mkdirs(target)
        paths = [os.path.join(root, f) for f in sorted(files) if not os.path.islink(os.path.join(root, f))]
        upload(target, paths)
        TOUCHED.update(paths)


def download_tree(dev, host, seen=None):
    """Brings dev back to host; returns the host paths the console has, or
    None when the folder could not be listed."""
    top = seen is None
    seen = set() if top else seen
    items = ls(dev)
    if items is None:
        return None if top else seen
    os.makedirs(host, exist_ok=True)
    for name, is_dir in items:
        if is_dir:
            download_tree(dev + "\\" + name, os.path.join(host, name), seen)
            continue
        path = os.path.join(host, name)
        seen.add(path)
        if own_output(path):
            continue
        data = download(dev, name)
        if data is None:
            continue
        try:
            with open(path, "rb") as f:
                if f.read() == data:
                    continue
        except OSError:
            pass
        with open(path, "wb") as f:
            f.write(data)
        TOUCHED.add(path)
    return seen


def file_hash(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for block in iter(lambda: f.read(1 << 20), b""):
            h.update(block)
    return h.hexdigest()


def state_get(name):
    try:
        with open(os.path.join(STATE, name)) as f:
            return f.read()
    except OSError:
        return None


def state_put(name, text):
    os.makedirs(STATE, exist_ok=True)
    with open(os.path.join(STATE, name), "w") as f:
        f.write(text)


def b64(s):
    import base64
    return q(base64.b64encode(s.encode()).decode())


def stop_app():
    api("DELETE", "/api/taskmanager/app?package=" + b64(PACKAGE), check=False)


def app_running():
    out = api("GET", "/api/resourcemanager/processes", check=False)
    try:
        return any(p.get("ImageName", "").lower() == "lba2cc.exe" for p in json.loads(out).get("Processes", []))
    except ValueError:
        return False


def local_state():
    """The app's LocalState as the app spells it, read once from its own log."""
    cached = state_get("localstate")
    if cached:
        return cached
    for name in ("adeline.log", "adeline.prev.log"):
        data = download("\\LocalState", name)
        m = re.search(rb"Config:\s+(\S.*\\LocalState\\)", data or b"")
        if m:
            path = m.group(1).decode()
            state_put("localstate", path)
            return path
    die("cannot learn the LocalState path: run the app once (tools/uwp/xbox-run.sh)")


def deployed_exe_size():
    r = subprocess.run([XBOX, "api", "GET", "/api/filesystem/apps/files?knownfolderid=DevelopmentFiles&path=" +
                        q("\\LooseApps\\lba2cc")], capture_output=True)
    if r.returncode != 0:
        return None
    for i in json.loads(r.stdout or b"{}").get("Items", []):
        if i["Name"].lower() == "lba2cc.exe":
            return i.get("FileSize")
    return None


def deploy_exe():
    if not os.path.isfile(EXE):
        die("no exe: %s (build it, or set LBA2_XBOX_EXE)" % EXE)
    h = file_hash(EXE)
    # The record alone is not enough: xbox-run.sh deploys its own build, and
    # the size of the console's copy says when that happened.
    if state_get("exe") == h and deployed_exe_size() == os.path.getsize(EXE):
        return h
    stop_app()
    api("POST", "/api/filesystem/apps/file?knownfolderid=DevelopmentFiles&path=" + q("\\LooseApps\\lba2cc"),
        "-F", "f=@" + EXE, "-o", "/dev/null")
    state_put("exe", h)
    note("sent %s" % EXE)
    return h


def check_game_dir(host_game):
    if state_get("gamecheck") == host_game:
        return
    hqr = os.path.join(host_game, "LBA2.HQR")
    if not os.path.isfile(hqr):
        die("LBA2_GAME_DIR has no LBA2.HQR: %s" % host_game)
    sizes = ls_sizes("\\LocalState\\Game") or {}
    if sizes.get("LBA2.HQR") != os.path.getsize(hqr):
        die("LBA2_GAME_DIR does not hold the console's game data (LBA2.HQR differs): %s" % host_game)
    state_put("gamecheck", host_game)


def normalize(text):
    text = re.sub(r"//+", "/", text)
    prev = None
    while prev != text:
        prev = text
        text = re.sub(r"/[^/ ;\"']+/\.\./", "/", text)
    return text


def physical(path):
    try:
        return os.path.realpath(path) if path and os.path.exists(path) else None
    except OSError:
        return None


class Translator:
    PATH_END = re.compile(r"[^\s;\"',]*")

    def __init__(self, local):
        self.local = local                      # Q:\...\LocalState\
        self.roots = []                         # (host spelling, device path, kind, index)
        self.named = {}                         # temp index -> set of relative paths named
        seen = set()
        temps = [os.environ.get("TMPDIR", "/tmp")]
        try:
            temps.append(subprocess.run(["getconf", "DARWIN_USER_TEMP_DIR"], capture_output=True,
                                        text=True).stdout.strip())
        except OSError:
            pass
        temps.append("/tmp")
        self.temps = []
        for t in temps:
            p = physical(t)
            if not p or p in seen:
                continue
            seen.add(p)
            i = len(self.temps)
            self.temps.append(p)
            dev = "%st\\run\\%d" % (local, i)
            for spelling in {normalize(t.rstrip("/")), p}:
                self.roots.append((spelling, dev, "tmp", i))
        game = os.environ.get("LBA2_GAME_DIR")
        if game:
            for spelling in {normalize(game.rstrip("/")), physical(game)}:
                if spelling:
                    self.roots.append((spelling, local + "Game", "game", 0))
        save = os.environ.get("LBA2_TEST_SAVE")
        if save:
            d = os.path.dirname(save)
            for spelling in {normalize(d), physical(d)}:
                if spelling:
                    self.roots.append((spelling, local + "t\\save", "save", 0))
        for spelling in {REPO, physical(REPO)}:
            self.roots.append((spelling, local + "t\\repo", "repo", 0))
        self.roots.sort(key=lambda r: -len(r[0]))
        self.repo_files = set()
        self.used = {}                          # device root -> the host spelling a run passed

    def arg(self, text):
        # A relative path means nothing in the app's folder: a whole argument
        # that names something here becomes absolute first.
        if text and not text.startswith("/") and not text.startswith("-") and os.path.exists(text):
            text = os.path.abspath(text)
        text = normalize(text)
        out, pos = [], 0
        while pos < len(text):
            best = None
            for spelling, dev, kind, i in self.roots:
                if text.startswith(spelling, pos):
                    end = pos + len(spelling)
                    if end == len(text) or text[end] in "/;\"' ,":
                        best = (spelling, dev, kind, i)
                        break
            if not best:
                out.append(text[pos])
                pos += 1
                continue
            spelling, dev, kind, i = best
            self.used.setdefault(dev, spelling)
            pos += len(spelling)
            # A whole argument that is a path may hold spaces; one inside a
            # command string ends at a space or a separator.
            whole = pos == len(spelling) and os.path.exists(text)
            rest = text[pos:] if whole else self.PATH_END.match(text, pos).group(0)
            pos += len(rest)
            rel = rest.lstrip("/")
            if kind == "tmp":
                self.named.setdefault(i, set()).add(rel)
            elif kind == "repo" and rel:
                self.repo_files.add(rel)
            out.append(dev + ("\\" + rel.replace("/", "\\") if rel else ""))
        return "".join(out)

    def back(self, text):
        # Each device root back to the spelling the caller used for it, or
        # failing that the one it was given (TMPDIR as set, not resolved).
        host_of = dict(self.used)
        for spelling, dev, kind, i in reversed(self.roots):
            if dev not in self.used and (dev not in host_of or spelling != physical(spelling)):
                host_of[dev] = spelling
        pairs = sorted(host_of.items(), key=lambda p: -len(p[0]))
        for dev, host in pairs:
            pattern = re.compile(re.escape(dev) + r"((?:[\\/][^\s\"';,]*)?)", re.IGNORECASE)
            text = pattern.sub(lambda m: host + m.group(1).replace("\\", "/"), text)
        return text


def cache_key(args, exe_hash):
    if args in (["--help"], ["--help-all"], ["--version"]):
        return hashlib.sha256((exe_hash + "\0" + "\0".join(args)).encode()).hexdigest()[:16]
    return None


def same_file(a, b):
    try:
        x, y = os.fstat(a), os.fstat(b)
    except OSError:
        return False
    return (x.st_dev, x.st_ino) == (y.st_dev, y.st_ino)


def main():
    args = sys.argv[1:]
    # One run at a time: the console runs one copy of the app, and a fixture
    # may start runs in the background. Later calls wait here.
    import fcntl
    os.makedirs(STATE, exist_ok=True)
    lock = open(os.path.join(STATE, "lock"), "w")
    fcntl.flock(lock, fcntl.LOCK_EX)
    exe_hash = deploy_exe()
    key = cache_key(args, exe_hash)
    if key:
        cached = state_get("cache-" + key)
        if cached is not None:
            rc, out, err = json.loads(cached)
            sys.stdout.write(out)
            sys.stderr.write(err)
            return rc

    local = local_state()
    tr = Translator(local)
    if os.environ.get("LBA2_GAME_DIR"):
        check_game_dir(os.environ["LBA2_GAME_DIR"])

    # SDL's dummy video driver is how some fixtures ask for a windowless run;
    # the UWP build has no SDL, and --headless is its way to say the same. Not
    # when the dummy audio driver asks for a sample device with sound on:
    # --headless opens none, and the console has a real one.
    wants_sound = os.environ.get("SDL_AUDIODRIVER") == "dummy" and "--no-audio" not in args
    if os.environ.get("SDL_VIDEODRIVER") == "dummy" and not wants_sound and "--headless" not in args:
        args = ["--headless"] + args
    dev_args = [tr.arg(a) for a in args]
    env = {}
    for v in ENV_VARS:
        if v in os.environ:
            env[v] = tr.arg(os.environ[v])
    # Never the console player's own lba2.cfg and saves: a run with no user dir
    # of its own gets one.
    if "LBA2_USER_DIR" not in env and not any(a == "--user-dir" or a.startswith("--user-dir=") for a in args):
        env["LBA2_USER_DIR"] = local + "t\\home"
    # `cmd > log 2>&1`: one file, so the console writes one, keeping the order
    # of lines across the two streams that fixtures read.
    one_stream = same_file(1, 2)
    if one_stream:
        env["LBA2_UWP_ONE_STREAM"] = "1"
    # The app reads each line into ARG_SIZE bytes and skips one that is longer.
    lines = {"args.txt": dev_args, "env.txt": ["%s=%s" % kv for kv in env.items()]}
    for name, items in lines.items():
        longest = max([len(a.encode()) for a in items] or [0])
        if longest > ARG_SIZE or any("\n" in a or "\r" in a for a in items):
            die("%s: a line does not fit (longest %d bytes, the app reads %d)" % (name, longest, ARG_SIZE))
    if len(dev_args) > MAX_ARGS:
        die("args.txt: %d arguments, the app reads %d" % (len(dev_args), MAX_ARGS))

    stop_app()
    remove_tree("\\LocalState\\t\\run")
    remove_tree("\\LocalState\\t\\home")
    for name in ("stdout.txt", "stderr.txt", "exit.txt"):
        remove("\\LocalState", name)

    # Named temp paths that exist go up; their folders come back afterwards.
    watch = []
    for i, rels in tr.named.items():
        for rel in sorted(rels):
            host = os.path.join(tr.temps[i], rel) if rel else tr.temps[i]
            dev = "\\LocalState\\t\\run\\%d" % i + ("\\" + rel.replace("/", "\\") if rel else "")
            if os.path.exists(host) and rel:
                upload_tree(host, dev)
            # A path straight in the temp root comes back alone; deeper, its
            # folder comes back, so a log or lba2.cfg next to it does too.
            folder_rel = rel if os.path.isdir(host) else os.path.dirname(rel)
            if folder_rel:
                watch.append((i, folder_rel, False))
            elif rel:
                watch.append((i, rel, True))
            mkdirs("\\LocalState\\t\\run\\%d" % i + ("\\" + folder_rel.replace("/", "\\") if folder_rel else ""))
    mkdirs("\\LocalState\\t\\home")
    # A named file brings the files that extend its name: the engine finds
    # a recording's savegame (x.rec.lba) next to x.rec without being told.
    repo = set(tr.repo_files)
    for rel in tr.repo_files:
        host = os.path.join(REPO, rel)
        if os.path.isfile(host):
            d, base = os.path.split(rel)
            repo.update(os.path.join(d, f) for f in os.listdir(os.path.dirname(host))
                        if f.startswith(base + "."))
    for rel in sorted(repo):
        host = os.path.join(REPO, rel)
        if os.path.exists(host):
            upload_tree(host, "\\LocalState\\t\\repo\\" + rel.replace("/", "\\"))
    save = os.environ.get("LBA2_TEST_SAVE")
    if save and any(k == "save" for _, _, k, _ in tr.roots):
        d = os.path.dirname(save)
        h = hashlib.sha256("".join(sorted(file_hash(os.path.join(d, f)) for f in os.listdir(d)
                                          if os.path.isfile(os.path.join(d, f)))).encode()).hexdigest()
        # Registering the app again wipes LocalState, so the record alone
        # is not enough.
        if state_get("save") != h or ls("\\LocalState\\t\\save") is None:
            remove_tree("\\LocalState\\t\\save")
            upload_tree(d, "\\LocalState\\t\\save")
            state_put("save", h)

    work = os.path.join(STATE, "run")
    os.makedirs(work, exist_ok=True)
    with open(os.path.join(work, "args.txt"), "w", newline="\n") as f:
        f.write("".join(a + "\n" for a in dev_args))
    with open(os.path.join(work, "env.txt"), "w", newline="\n") as f:
        f.write("".join("%s=%s\n" % kv for kv in env.items()))
    upload("\\LocalState", [os.path.join(work, "args.txt"), os.path.join(work, "env.txt")])

    stopped = {"by": None}

    def on_signal(signum, _frame):
        stopped["by"] = signum
        raise KeyboardInterrupt

    for s in (signal.SIGTERM, signal.SIGINT, signal.SIGHUP):
        signal.signal(s, on_signal)

    rc = None
    try:
        r = subprocess.run([XBOX, "launch", PACKAGE], capture_output=True, text=True)
        if r.returncode != 0:
            die("launch failed (is a user signed in on the console?): " + r.stderr.strip())
        start = time.time()
        seen = False
        live_sizes = {}
        while True:
            running = app_running()
            seen = seen or running
            if not running and (seen or time.time() - start > START_WAIT):
                break
            if ("exit.txt", False) in (ls("\\LocalState") or []):
                break
            for i, rel, single in watch:
                if not single:
                    sync_live("\\LocalState\\t\\run\\%d\\" % i + rel.replace("/", "\\"),
                              os.path.join(tr.temps[i], rel), live_sizes)
            time.sleep(POLL)
        # The app may still be shutting down after writing exit.txt.
        while app_running() and time.time() - start < START_WAIT + 600:
            time.sleep(POLL)
    except KeyboardInterrupt:
        stop_app()
        rc = 128 + (stopped["by"] or signal.SIGINT)

    out = (download("\\LocalState", "stdout.txt") or b"").decode(errors="replace")
    # An exe from before LBA2_UWP_ONE_STREAM still writes stderr.txt.
    err = (download("\\LocalState", "stderr.txt") or b"").decode(errors="replace")
    sys.stdout.write(tr.back(out))
    sys.stdout.flush()
    sys.stderr.write(tr.back(err))
    if rc is None:
        code = download("\\LocalState", "exit.txt")
        if code is None:
            note("no exit code: the app crashed, or was ended before exit()")
            rc = 134
        else:
            rc = int(code.decode().strip() or "1") & 0xFF

    for i, rel, single in watch:
        dev = "\\LocalState\\t\\run\\%d" % i
        host = os.path.join(tr.temps[i], rel)
        if single:
            data = download(dev, rel)
            if data is not None:
                if not own_output(host):
                    with open(host, "wb") as f:
                        f.write(data)
            elif host in TOUCHED and os.path.isfile(host):
                os.remove(host)
        else:
            seen = download_tree(dev + "\\" + rel.replace("/", "\\"), host)
            # Deleted on the console, so deleted here: a run that cleans up
            # after itself must look as if it did.
            for path in sorted(TOUCHED) if seen is not None else []:
                if path.startswith(host + os.sep) and path not in seen and os.path.isfile(path):
                    os.remove(path)
    for name in ("args.txt", "env.txt"):
        remove("\\LocalState", name)

    if key and rc is not None:
        state_put("cache-" + key, json.dumps([rc, tr.back(out), tr.back(err)]))
    return rc


if __name__ == "__main__":
    sys.exit(main())
