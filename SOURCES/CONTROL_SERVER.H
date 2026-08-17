#ifndef CONTROL_SERVER_H
#define CONTROL_SERVER_H

/*
 * Interactive control socket: a loopback line server in front of the console
 * command bus, so a driver outside the process can probe and steer a *running*
 * engine instead of scripting the whole run before boot.
 *
 * What it is for: probe loops where the next command depends on what the last
 * one showed. Searching the camera cvars for the value that fixes a behaviour,
 * say, where every probe otherwise costs a boot, a load and a walk back to the
 * scene. Paying that setup once per session instead of once per probe is the
 * whole point of the channel.
 *
 * What it is not for: anything that can be scripted up front. --exec-at already
 * does that, stays deterministic and replays; this one deliberately does not,
 * because the commands arrive at whatever tick the driver sent them.
 *
 * Dead unless the build defines LBA2_CONTROL_SERVER *and* the run passes
 * --listen. It binds 127.0.0.1 only and runs arbitrary console commands by
 * design, which is exactly why it has no business in a shipped binary.
 *
 * Berkeley sockets and Winsock, behind one seam in the .CPP. The automation this
 * exists to serve runs on Windows too, and a diagnostic channel that is missing on
 * the platform a bug was reported from is the wrong half to have.
 *
 * Protocol: one command per line in; that command's console output back
 * verbatim, terminated by a line reading <<END>>. One client at a time.
 * Anything the engine logs while the command runs lands in the response too,
 * since the log fans out to the console: a warning raised by the command is
 * part of its answer.
 *
 * A command runs once per PRESENTED frame, which is not once per simulation tick:
 * presents outnumber ticks by one to two orders of magnitude here. So a command
 * that changes simulation state and a command that reads the consequences can both
 * land inside one tick, and the read returns what was true before the write. It
 * does not look like a race: it looks like a stable, repeatable measurement of the
 * wrong thing. Let a tick elapse between the two (a short wait, or drive the sim
 * with a pinned throttle and wait that long) whenever the read depends on anything
 * the object loop computes: zones, collision, animation, position.
 *
 * A session left to free-run is not a session a player would recognise. Headless
 * with nothing capping the loop, the engine renders on the order of 1500 frames a
 * second, tens of rendered frames per simulation tick. Harness input is metered
 * in sim ticks, so `input up 120` spends itself in a fraction of the wall-clock
 * time a driver expects, and anything rate-coupled behaves accordingly. Pin the
 * throttle for a probe session (`fixedtimestep 40` over the socket, or
 * --fixed-timestep on the command line) and read timings against that.
 *
 * Threading: none. Control_TickHook polls it once per tick, so commands run on
 * the game thread at the same point --exec-at fires and reads of engine globals
 * are exactly as safe as they are there. The cost is that a modal inner loop
 * (cinematic, dialogue, menu) does not reach the hook, so the socket goes quiet
 * until the main loop resumes.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* Open the listener on 127.0.0.1:<port>. Returns 0 on success, non-zero if the
   socket could not be opened (already-bound port, unsupported build). Failure is
   logged and non-fatal: the run continues without the channel. */
int ControlServer_Start(int port);

/* Accept a pending client, execute whatever whole lines have arrived, and write
   each response back. Non-blocking throughout: with no client and no input this
   is two failed syscalls, which is why it can sit in the per-tick hook. */
void ControlServer_Poll(void);

/* Close the client and the listener. Idempotent. */
void ControlServer_Stop(void);

/* Non-zero if the listener is open. */
int ControlServer_IsRunning(void);

#ifdef __cplusplus
}
#endif

#endif /* CONTROL_SERVER_H */
