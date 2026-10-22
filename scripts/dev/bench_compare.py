#!/usr/bin/env python3
"""Compare the frame rate of two lba2cc builds, scene by scene.

    scripts/dev/bench_compare.py --game-dir /path/to/game/data \\
        --arm 0.13.0=/path/to/old/lba2cc --arm main=/path/to/new/lba2cc

By default each run plays the attract reel (cube 193 with --demo): a fixed
script that visits the same scenes in the same order on every build, so both
sides draw the same frames. It plays twice per round, once with the scripted
cameras and once with the Auto camera, which redraws the whole outdoor view
every frame. --save switches to a scripted walk from a save instead.

Every run is held to a fixed step (--fixed-dt) with audio off, and the builds
take turns in the order A B, B A, A B ..., so a machine that warms up mid-session
does not favour one side. By default the game runs in a real window with vsync
off, so the numbers include presenting to the screen; the window opens on top
for each run. --headless swaps in SDL's dummy driver.

Every frame is written by `perftrace log`, which both builds need. It is on main
from the commit that added this script; for an older tag, apply that commit's
changes to SOURCES/PERFTRACE.{H,CPP} and SOURCES/CONSOLE/CONSOLE_CMD.CPP before
building it.

Per scene the report gives, for each build, the median over runs of:
  avg   frames divided by the time they took
  1%    the 1% low: the frame rate of the 99th-percentile frame
  min   the slowest frame
  max   the fastest frame
  eng   engine milliseconds per frame: frame time minus present time, so the
        game's own work without the wait for the screen. In a window the
        present can dominate (macOS paces a window's presents even with vsync
        off), and then the fps columns show the system, and this one the build.
A ratio marked * means the two builds' per-run values do not overlap.
Left out: the first frames of each visit to a scene (--skip), since one of them
carries the scene load, and every frame that presented more than once. Those
are fades and waits the game runs inside one frame on the real clock (a fade
takes half a second however fast the machine is); the report counts them in
the "waits" column instead. Numbers are wall-clock time on this machine: use
them to compare two builds on one machine, never two machines.

Every run gets its own user folder, so the player's settings and saves are never
read or written. Needs python 3.8+ and no packages.
"""

import argparse
import json
import os
import re
import shutil
import statistics
import subprocess
import sys
import tempfile
import time

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

# Forward, a left turn, forward, a right turn, forward, a long left. Sim ticks,
# relative to --start, so it is the same walk at any frame rate.
DEFAULT_SEQ = (
    "10:up:400/410:left:120/530:up:400/930:right:120/1050:up:400/1450:left:200"
)


def parse_args():
    p = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    p.add_argument(
        "--arm",
        action="append",
        required=True,
        metavar="NAME=BIN",
        help="a build to time; give exactly two",
    )
    p.add_argument("--game-dir", help="retail game data")
    p.add_argument(
        "--runs", type=int, default=3, help="runs per build per workload (default 3)"
    )
    p.add_argument(
        "--ticks",
        type=int,
        default=20000,
        help="ticks per reel run (default 20000), or timed ticks per walk",
    )
    p.add_argument(
        "--camera",
        choices=["both", "scripted", "auto"],
        default="both",
        help="reel cameras: the scripted shots, the Auto camera, or both (default)",
    )
    p.add_argument(
        "--headless",
        action="store_true",
        help="no window: SDL's dummy driver, so present is not a real swap",
    )
    p.add_argument("--resolution", help="WxH; default is what the engine picks")
    p.add_argument("--fixed-dt", type=int, default=16, help="ms per tick (default 16)")
    p.add_argument(
        "--skip",
        type=int,
        default=3,
        help="frames left out at the start of each scene visit (default 3)",
    )
    p.add_argument(
        "--save",
        action="append",
        help="walk from this save instead of playing the reel; repeatable",
    )
    p.add_argument(
        "--seq",
        action="append",
        help="input seq for each --save, in the same order, or one for all",
    )
    p.add_argument(
        "--start",
        type=int,
        default=240,
        help="walk: tick at which the walk and the timing start (default 240)",
    )
    p.add_argument(
        "--exec", dest="extra", default="", help="extra console commands run at boot"
    )
    p.add_argument(
        "--cool", type=float, default=2.0, help="seconds to rest between runs"
    )
    p.add_argument(
        "--out", help="folder for raw logs and results.json (default: a temp folder)"
    )
    p.add_argument(
        "--timeout", type=int, default=900, help="seconds before a run is killed"
    )
    p.add_argument(
        "--from",
        dest="from_dir",
        help="rebuild the report from an earlier --out folder instead of running; "
        "give the same --arm names (paths optional) and workload options",
    )
    a = p.parse_args()
    if not a.from_dir and not a.game_dir:
        p.error("--game-dir is needed to run")

    if len(a.arm) != 2:
        p.error("give exactly two --arm NAME=BIN")
    a.arms = []
    for spec in a.arm:
        name, sep, path = spec.partition("=")
        if a.from_dir and name:
            a.arms.append((name, os.path.abspath(path) if path else ""))
            continue
        if not sep or not name or not os.access(path, os.X_OK):
            p.error(f"--arm {spec!r}: want NAME=BIN with an executable BIN")
        a.arms.append((name, os.path.abspath(path)))
    if a.arms[0][0] == a.arms[1][0]:
        p.error("the two arms need different names")

    a.workloads = []
    if a.save:
        seqs = a.seq or [DEFAULT_SEQ]
        if len(seqs) == 1:
            seqs = seqs * len(a.save)
        if len(seqs) != len(a.save):
            p.error("give one --seq per --save, or a single one for all")
        for s, q in zip(a.save, seqs):
            if not os.path.isfile(s):
                p.error(f"no save at {s}")
            name = os.path.splitext(os.path.basename(s))[0]
            a.workloads.append(
                {"name": f"walk {name}", "save": os.path.abspath(s), "seq": q}
            )
    else:
        if a.seq:
            p.error("--seq needs --save")
        if a.camera in ("both", "scripted"):
            a.workloads.append({"name": "reel", "follow": False})
        if a.camera in ("both", "auto"):
            a.workloads.append({"name": "reel, Auto camera", "follow": True})
    return a


def scene_names():
    """Cube number -> (location, mode), from docs/SCENES.md when it is there."""
    names = {}
    try:
        with open(os.path.join(REPO, "docs", "SCENES.md")) as f:
            for line in f:
                m = re.match(
                    r"\|\s*(\d+)\s*\|\s*([^|]+?)\s*\|\s*(interior|exterior)\s*\|", line
                )
                if m and int(m.group(1)) not in names:
                    names[int(m.group(1))] = (m.group(2), m.group(3))
    except OSError:
        pass
    return names


def build_cmd(a, bin_path, w, log):
    cmd = [bin_path, "--no-autosave", "--fixed-dt", str(a.fixed_dt)]
    # No audio either way: a live audio thread branches the simulation.
    cmd += ["--headless"] if a.headless else ["--no-audio", "--vsync", "off"]
    if a.resolution:
        cmd += ["--resolution", a.resolution]
    capture = f"perftrace log {log}; perftrace start"
    boot = "skipmodals 1"
    if a.extra:
        boot += "; " + a.extra
    if "save" in w:
        cmd += [
            "--load",
            w["save"],
            "--exec",
            f"cam_follow 1; godmode 1; {boot}",
            "--exec-at",
            str(a.start),
            f"input seq {w['seq']}; {capture}",
        ]
        ticks = a.start + a.ticks
    else:
        follow = "cam_follow 1; " if w["follow"] else ""
        cmd += ["--exec", f"cube 193; {follow}{boot}; {capture}", "--demo"]
        ticks = a.ticks
    return cmd, ticks


def run_once(a, bin_path, w, workdir):
    """One run. Returns its frames as (cube, tag, frame_us, present_us, presents) and its
    end state."""
    user = os.path.join(workdir, "user")
    os.makedirs(user)
    log = os.path.join(workdir, "frames.txt")
    state = os.path.join(workdir, "state.json")
    cmd, ticks = build_cmd(a, bin_path, w, log)
    cmd += ["--tick", str(ticks), "--dump-state", state, "--exit"]

    # The user and game folders go in the environment, not as flags: the engine's
    # own command line reads the first argument the harness leaves behind as a save
    # name, so a --user-dir there gets "loaded" too.
    env = dict(os.environ, LBA2_USER_DIR=user, LBA2_GAME_DIR=a.game_dir)
    # A connected pad would steer the hero mid-run (tests/automation/lib.sh does the same).
    env["SDL_GAMECONTROLLER_IGNORE_DEVICES_EXCEPT"] = "0xFFFF/0xFFFF"
    out = os.path.join(workdir, "engine.log")
    with open(out, "w") as f:
        rc = subprocess.run(
            cmd,
            stdout=f,
            stderr=subprocess.STDOUT,
            env=env,
            timeout=a.timeout,
            check=False,
        ).returncode
    if rc != 0:
        raise RuntimeError(f"engine exited {rc}; see {out}")
    if not os.path.exists(log):
        raise RuntimeError(
            f"no frame log; does this build have `perftrace log`? see {out}"
        )
    return read_run(workdir)


def read_run(workdir):
    """A finished run's frames and end state, from its folder."""
    log = os.path.join(workdir, "frames.txt")
    state = os.path.join(workdir, "state.json")
    frames = []
    with open(log) as f:
        for line in f:
            if line.startswith("#"):
                continue
            f6 = line.split()
            if len(f6) != 6:
                raise RuntimeError(
                    f"{log}: want 6 fields per frame, got {line!r}; "
                    "this build's `perftrace log` is older"
                )
            cube, tag, frame, _scene, present, presents = f6
            frames.append((int(cube), tag, int(frame), int(present), int(presents)))
    with open(state) as f:
        st = json.load(f)
    hero = st["hero"]
    end = {
        "cube": st["scene"]["cube"],
        "hero": [hero["x"], hero["y"], hero["z"], hero["beta"]],
        "life": hero["life"],
        "size": f"{st['scene']['render_w']}x{st['scene']['render_h']}",
    }
    return frames, end


def split_scenes(frames, skip):
    """Frame times per cube, without the first `skip` frames of each visit and
    without fade or wait frames (more than one present), which are counted."""
    per = {}
    order = []
    prev = None
    run = 0
    for cube, tag, frame, present, presents in frames:
        run = run + 1 if cube == prev else 0
        prev = cube
        if cube not in per:
            per[cube] = {"tag": tag, "frame": [], "waits": 0}
            order.append(cube)
        if presents > 1:
            per[cube]["waits"] += 1
        elif run >= skip:
            per[cube]["frame"].append((frame, present))
    return order, per


def fps_stats(frames):
    """Frame-rate figures from (frame_us, present_us) pairs."""
    if not frames:
        return None
    s = sorted(f for f, _ in frames)
    n = len(s)
    return {
        "eng": sum(f - p for f, p in frames) / n / 1000,
        "frames": n,
        "avg": n * 1e6 / sum(s),
        "low1": 1e6 / s[min(n - 1, int(n * 0.99))],
        "min": 1e6 / s[-1],
        "max": 1e6 / s[0],
    }


COLS = [("avg", "avg"), ("low1", "1%"), ("min", "min"), ("max", "max")]


def report(w, names, arms, runs, skip):
    """Print the per-scene table for one workload."""
    (na, _), (nb, _) = arms
    order = []
    for r in runs[na] + runs[nb]:
        for c in r["order"]:
            if c not in order:
                order.append(c)
    order.append("all")

    def vals(name, cube, key):
        return [r["stats"][cube][key] for r in runs[name] if r["stats"].get(cube)]

    def med(name, cube, key):
        v = vals(name, cube, key)
        return statistics.median(v) if v else None

    def ratio(cube, key):
        va, vb = med(na, cube, key), med(nb, cube, key)
        if not va or not vb:
            return f"{'-':>10}"
        a_, b_ = vals(na, cube, key), vals(nb, cube, key)
        apart = len(a_) > 1 and (max(a_) < min(b_) or max(b_) < min(a_))
        return f"{vb / va:9.3f}" + ("*" if apart else " ")

    width = max([len(names.get(c, ("",))[0]) for c in order if c != "all"] + [8])
    width = min(width, 34)
    head = f"  {'cube':>4} {'scene':<{width}} {'frames':>13} {'waits':>7} "
    head += "".join(f"{na[:9] + ' ' + lbl:>13}" for _, lbl in COLS)
    head += "".join(f"{nb[:9] + ' ' + lbl:>13}" for _, lbl in COLS)
    head += f"{'avg ' + nb[:5] + '/' + na[:5]:>16}"
    head += f"{na[:9] + ' eng':>13}{nb[:9] + ' eng':>13}{'eng ' + nb[:5] + '/' + na[:5]:>16}"
    print(
        f"\n{w['name']}: frames per second, median of {len(runs[na])} runs "
        f"(left out: the first {skip} frames of each scene visit, and fade/wait frames)"
    )
    print(f"  frames and waits are {na} / {nb}")
    print(head)
    for cube in order:
        if cube == "all":
            label = "whole run"
        else:
            tags = (
                runs[na][0]["tags"]
                if runs[na][0]["tags"].get(cube)
                else runs[nb][0]["tags"]
            )
            nm, mode = names.get(cube, ("?", tags.get(cube, "")))
            ext = " (ext)" if mode in ("exterior", "ext") else ""
            label = nm[: width - len(ext)] + ext

        def pair(key):
            va, vb = med(na, cube, key), med(nb, cube, key)
            return "/".join("-" if v is None else f"{v:.0f}" for v in (va, vb))

        line = f"  {'' if cube == 'all' else cube:>4} {label:<{width}} {pair('frames'):>13} "
        line += f"{pair('waits'):>7} "
        for name in (na, nb):
            for key, _ in COLS:
                v = med(name, cube, key)
                line += f"{v:13.1f}" if v is not None else f"{'-':>13}"
        line += f"{ratio(cube, 'avg'):>16}"
        for name in (na, nb):
            v = med(name, cube, "eng")
            line += f"{v:13.3f}" if v is not None else f"{'-':>13}"
        line += f"{ratio(cube, 'eng'):>16}"
        print(line)
    spread = {name: [r["stats"]["all"]["avg"] for r in runs[name]] for name in (na, nb)}
    print(
        "  whole-run avg fps per run: "
        + ", ".join(
            f"{name} " + " ".join(f"{v:.1f}" for v in spread[name]) for name in (na, nb)
        )
    )
    lo_a, hi_a = min(spread[na]), max(spread[na])
    lo_b, hi_b = min(spread[nb]), max(spread[nb])
    if lo_a <= hi_b and lo_b <= hi_a:
        print(
            "  the two builds' run ranges overlap: the whole-run difference is within noise"
        )


def main():
    a = parse_args()
    out = a.from_dir or a.out or tempfile.mkdtemp(prefix="lba2-bench-")
    os.makedirs(out, exist_ok=True)
    names = scene_names()
    (na, ba), (nb, bb) = a.arms
    results = {
        "arms": dict(a.arms),
        "game_dir": a.game_dir,
        "fixed_dt": a.fixed_dt,
        "headless": a.headless,
        "resolution": a.resolution,
        "ticks": a.ticks,
        "skip": a.skip,
        "workloads": [],
    }
    ok = True

    for w in a.workloads:
        order = []
        for r in range(a.runs):
            pair = [(na, ba), (nb, bb)] if r % 2 == 0 else [(nb, bb), (na, ba)]
            order += [(r, n, b) for n, b in pair]
        runs = {na: [], nb: []}
        for r, name, path in order:
            tag = re.sub(r"[^A-Za-z0-9]+", "-", w["name"]).strip("-")
            wd = os.path.join(out, f"{tag}-{name}-{r}")
            t0 = time.time()
            if a.from_dir:
                frames, end = read_run(wd)
            else:
                shutil.rmtree(wd, ignore_errors=True)
                os.makedirs(wd)
                frames, end = run_once(a, path, w, wd)
            scene_order, per = split_scenes(frames, a.skip)
            stats = {c: fps_stats(per[c]["frame"]) for c in scene_order}
            stats["all"] = fps_stats([f for c in scene_order for f in per[c]["frame"]])
            for c in scene_order:
                if stats[c]:
                    stats[c]["waits"] = per[c]["waits"]
            stats["all"]["waits"] = sum(per[c]["waits"] for c in scene_order)
            runs[name].append(
                {
                    "order": scene_order,
                    "stats": stats,
                    "end": end,
                    "tags": {c: per[c]["tag"] for c in scene_order},
                    "present_ms": sum(f[3] for f in frames) / len(frames) / 1000,
                }
            )
            print(
                f"  {w['name']:<18} {name:<10} run {r}: {stats['all']['avg']:7.1f} fps avg, "
                f"{len(frames)} frames, {end['size']}, {time.time() - t0:.0f} s",
                flush=True,
            )
            if not a.from_dir:
                time.sleep(a.cool)

        report(w, names, a.arms, runs, a.skip)

        # Same script, same ticks: each build must end the same way on every run,
        # or the runs drew different frames.
        for name in (na, nb):
            ends = {json.dumps(r["end"], sort_keys=True) for r in runs[name]}
            if len(ends) != 1:
                print(
                    f"  WARNING: {name} ended {len(ends)} different ways across runs; "
                    "the run does not reproduce"
                )
                ok = False
        ea, eb = runs[na][0]["end"], runs[nb][0]["end"]
        if ea["size"] != eb["size"]:
            print(
                f"  WARNING: the builds rendered at different sizes ({ea['size']} vs "
                f"{eb['size']}); pass --resolution"
            )
            ok = False
        if [r["order"] for r in runs[na][:1]] != [r["order"] for r in runs[nb][:1]]:
            print("  WARNING: the builds visited different scenes")
            ok = False
        elif ea != eb:
            print(
                "  note: the builds end in different states, so some actors moved "
                "differently; the scenes and their order match"
            )
        if any(r["end"]["life"] <= 0 for r in runs[na] + runs[nb]) and "save" in w:
            print(
                "  WARNING: the hero ends the walk dead; pick a walk that keeps them alive"
            )
            ok = False
        results["workloads"].append({**w, "runs": runs})
        print(flush=True)

    with open(os.path.join(out, "results.json"), "w") as f:
        json.dump(results, f, indent=1)
    print(f"raw logs and results.json in {out}")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
