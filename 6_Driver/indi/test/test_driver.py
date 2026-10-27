#!/usr/bin/env python3
# SOURCE: 6_Driver/indi/README.md (§ Ce que la CI démontre) — le driver joué contre un faux contrôleur sur un pty
# AUTHOR: engineer
# DATE: 2026-10-19
# STATUS: actif — joué par 6_Driver/indi/test.sh, job `indi` de la CI
"""Plays an INDI driver binary against a fake controller on a pty, through the driver's own stdin and stdout.

    test_driver.py <driver binary> <kind> <suite>

kind, what the binary is built from: `upstream` (pinefeat_cef at the commit named in UPSTREAM.md), `fixes` (upstream
and the series patches/fixes), `final` (fixes and the series patches/basilisk: the files of this directory).
suite: `pinefeat`, a fake Pinefeat controller (the letters `v f r e a d s c`, `er` to any other); `basilisk`, a fake
Basilisk board that reports ext=1 (final only).

Each check of the pinefeat suite that names a fix shows the defect that fix corrects: it must pass on fixes and final,
and fail on upstream with the symptom it declares, a fragment of its failure (PINEFEAT). A check whose defect gives no
deterministic symptom on upstream declares none, and is not played there. Every other check is upstream's own behaviour, played on all three:
on final it shows that a Pinefeat controller sees the extended set only as one `t` per connection.

The fake controllers are the protocol of 7_Docs/PROTOCOL.md (Basilisk) and of upstream's own reading of its replies
(Pinefeat); their timings are made up (a move takes 1 s), nothing is measured on hardware.
"""

import itertools
import os
import random
import re
import select
import subprocess
import sys
import tempfile
import threading
import time
import tty
import xml.etree.ElementTree as ET

DEVICE = "Pinefeat EF Lens Controller"
EXT_PROPERTIES = ["APERTURE", "FOCUS_MARK", "FOCUS_MARK_CTL", "LED_BRIGHTNESS", "LENS_REBOOT", "LENS_INFO",
                  "FOCUS_TEMPERATURE", "FOCUS_HUMIDITY", "FOCUS_DEW_POINT", "FOCUS_ABORT_MOTION", "BOARD_RESTART"]
EXT_LETTERS = re.compile(r"^(t|o|j|js|jg|jx|k\d*|b|i|w|n|z|g|q|x)$")
# A command fails within BOUND of the fake receiving it, whatever the controller keeps sending: CEF_TIMEOUT (3 s,
# upstream's value) from the command, and 2 s for the error to reach the test (written by the driver, read through a
# pipe and an XML parser), well beyond what a loaded runner adds to it; the defects it shows hold the driver for tens
# of seconds, or for ever.
BOUND = 3.0 + 2.0
FLOOD = [b"0123456789abcdef0123456789abcdef"]      # 32 bytes every 5 ms, without a line end: a flow that doesn't stop
ROM_LINES = [b"ESP-ROM:esp32c3-api1-20210207\r\n", b"Build:Feb  7 2021\r\n",
             b"rst:0x15 (USB_UART_CHIP_RESET),boot:0xd (SPI_FAST_FLASH_BOOT)\r\n", b"SPIWP:0xee\r\n",
             b"mode:DIO, clock div:1\r\n", b"load:0x3fcd5820,len:0x1714\r\n", b"entry 0x403cc71c\r\n"]


class Fake(threading.Thread):
    """The controller at the other end of a pty. Each command line received is logged (received), and answered by
    answer(): a str is a reply line; a list holds bytes written as they come, and (delay, function) called later."""

    def __init__(self, basilisk, version):
        super().__init__(daemon=True)
        self.master, self.slave = os.openpty()
        tty.setraw(self.slave)
        self.port = os.ttyname(self.slave)
        self.basilisk = basilisk
        self.version = version
        self.lock = threading.Lock()
        self.received = []
        self.lens = True
        self.pos = 16384
        self.limits = (13873, 30738) if basilisk else (0, 25000)
        self.target = None
        self.move_end = 0.0
        self.move_seconds = 1.0     # how long a move by `f` takes
        self.move_result = "-"
        self.next_result = "ok"
        self.mark = "16340"
        self.led = 100
        self.name = "SAMYANG AF 135mm F1.8"
        self.climate = "21.37 48.02 9.91"
        self.aperture = "5.6"
        self.reset = "poweron"      # `x`: the cause of the board's last restart, and when it restarted
        self.boot = time.monotonic()
        self.restarts = []          # (cause, ms): at each next `x`, the board has restarted for that cause ms ago
        self.delay = 0.0            # each reply delayed at random up to this (s): a part before the board answers
        self.rng = random.Random(704)   # (and reads its clock, `x`), the rest after; a board or a link slow at times
        self.lose_v = 0             # the next `v` sent while the board restarts: its ROM, no reply
        self.log_lines = False      # the board's log on (LOG ON): `* …` lines before each reply, one cut in two
        self.stale = None           # (command, bytes, rest): bytes written 20 ms after the next reply to command, the
        self.stale_rest = None      # last of a refresh, without their line end; the next `e`, the first command of the
        self.stale_hit = None       # next refresh, finds the rest of that line before its reply (its index here)
        self.script = {}            # command -> one-off answer
        self.endless_on = None      # (command, chunks, period): that command starts the endless input (endless())
        self.endless_from = None    # when the fake received it
        self.flow = None            # the thread that writes the endless input, and the event that stops it
        self.flow_stop = threading.Event()
        self.stop = False
        self.start()

    def run(self):
        buf = b""
        while not self.stop:
            try:
                data = os.read(self.master, 4096)
            except OSError:
                return
            buf += data
            while b"\n" in buf:
                line, buf = buf.split(b"\n", 1)
                cmd = line.decode(errors="replace").strip("\r")
                with self.lock:
                    self.received.append((time.monotonic(), cmd))
                    before = self.rng.uniform(0, self.delay)
                    after = self.rng.uniform(0, self.delay - before)
                time.sleep(before)
                with self.lock:
                    out = self.answer(cmd)
                time.sleep(after)
                self.send(out)

    def send(self, out):
        if out is None:
            return
        if isinstance(out, str):
            out = [out.encode() + b"\n"]
        for item in out:
            if isinstance(item, tuple):
                threading.Timer(item[0], item[1]).start()
            else:
                self.write(item)

    def write(self, data):
        try:
            os.write(self.master, data)
        except OSError:
            pass

    def endless(self, chunks, period):
        """From now on the controller answers nothing: it writes chunks one after the other, cycling, one every period
        seconds, until end_endless() or close(). A chunk is written only when the pty has room for it, so that the
        writer never blocks: a fast flow is as fast as the driver reads."""
        def flow():
            for chunk in itertools.cycle(chunks):
                try:
                    if select.select([], [self.master], [], 0)[1]:
                        os.write(self.master, chunk)
                except (OSError, ValueError):
                    return
                if self.flow_stop.wait(period):
                    return
        self.flow = threading.Thread(target=flow, daemon=True)
        self.flow.start()

    def end_endless(self):
        if self.flow is not None:
            self.flow_stop.set()
            self.flow.join()
            self.flow = None

    def close(self):
        self.stop = True
        self.end_endless()
        for fd in (self.slave, self.master):
            try:
                os.close(fd)
            except OSError:
                pass

    def commands(self, since=0.0):
        with self.lock:
            return [c for t, c in self.received if t >= since]

    def received_from(self, since):
        with self.lock:
            return [(t, c) for t, c in self.received if t >= since]

    def times(self, cmd, since=0.0):
        with self.lock:
            return [t for t, c in self.received if c == cmd and t >= since]

    def moving(self):
        if self.target is not None and time.monotonic() >= self.move_end:
            self.pos = self.target
            self.target = None
            self.move_result = self.next_result
            self.next_result = "ok"
        return self.target is not None

    def start_move(self, target, seconds=1.0):
        self.target = target
        self.move_end = time.monotonic() + seconds

    def restart(self, cause, resume):
        """The board restarts, the port open: `x` reports the new cause and a time from 0, then the board resumes its
        lens, `e` at y for resume seconds (PROTOCOL.md, lines `x` and `e`). Returns when it restarted."""
        with self.lock:
            self.reset = cause
            self.boot = time.monotonic()
            self.start_move(self.pos, resume)
            return self.boot

    def answer(self, cmd):
        if self.flow is not None:
            return None
        if self.endless_on is not None and self.endless_on[0] == cmd:
            _, chunks, period = self.endless_on
            self.endless_on = None
            self.endless_from = self.received[-1][0]
            self.endless(chunks, period)
            return None
        prefix = []
        if self.stale_rest is not None and cmd == "e":
            prefix.append(self.stale_rest)
            self.stale_rest = None
            self.stale_hit = len(self.received) - 1
        if self.basilisk and self.log_lines:
            prefix.append(b"* rx 05 len=65 ring=focus\n")
            prefix.append(b"* tx 03 seq=118 ap=0x0E3C focus=0x4000 cmd=00 01 02 03 04 05 06 07 08 09 0A 0B\n")
        reply = self.reply(cmd)
        if reply is None:
            return prefix or None
        if isinstance(reply, str):
            reply = [reply.encode() + b"\n"]
        out = prefix + reply
        if self.stale is not None and self.stale[0] == cmd:
            _, first, rest = self.stale
            self.stale = None
            out.append((0.02, lambda: self.write_stale(first, rest)))
        return out

    def write_stale(self, first, rest):
        with self.lock:
            self.write(first)
            self.stale_rest = rest

    def reply(self, cmd):
        if cmd in self.script:
            return self.script.pop(cmd)
        moving = self.moving()
        if cmd == "v":
            if self.lose_v:
                self.lose_v -= 1
                return ROM_LINES
            return self.version
        if not self.basilisk:
            return self.pinefeat(cmd, moving)
        return self.board(cmd, moving)

    def common(self, cmd, moving):
        """The letters both controllers serve, `f r e a d s c`."""
        if cmd == "e":
            return "y" if moving else "n"
        if cmd == "f":
            return str(self.pos) if self.lens else "nc"
        if cmd == "r":
            return "%d-%d" % self.limits if self.lens else "nc"
        if cmd == "a":
            return "1.8-22" if self.lens else "nc"
        if cmd == "d":
            return ("-" if self.basilisk else "1.25") if self.lens else "nc"
        if re.fullmatch(r"s\d", cmd):
            return "ok"
        if re.fullmatch(r"a[+-]?[0-9.]+", cmd):
            return "ok"
        m = re.fullmatch(r"f([+-]?)(\d+)", cmd)
        if m:
            if not self.lens:
                return "nc"
            n = int(m.group(2))
            target = self.pos + n if m.group(1) == "+" else self.pos - n if m.group(1) == "-" else n
            if not self.limits[0] <= target <= self.limits[1]:
                return "er range limits"
            self.start_move(target, self.move_seconds)
            return "ok"
        return None

    def pinefeat(self, cmd, moving):
        reply = self.common(cmd, moving)
        if reply is not None:
            return reply
        if cmd == "c":
            if not self.lens:
                return "nc"
            self.limits = (0, 25000)
            self.start_move(self.pos, 1.0)
            return "ok"
        return "er"

    def board(self, cmd, moving):
        reply = self.common(cmd, moving)
        if reply is not None:
            return reply
        if cmd == "t":
            return "ext=1 present=1 boot_state=ready busy=- power_off=0 last_op=-:ok mf=0 oss=0 ring=focus"
        if cmd == "c":
            return "ok" if self.lens else "nc"
        if cmd == "g":
            return self.move_result
        if cmd == "q":
            if self.target is not None:
                self.pos = (self.pos + self.target) // 2
                self.target = None
                self.move_result = "aborted"
            return "ok"
        if cmd == "o":
            return self.aperture
        if cmd == "j":
            return self.mark if self.lens else "er nolens"
        if cmd == "js":
            self.mark = str(self.pos)
            return "ok"
        if cmd == "jx":
            self.mark = "-"
            return "ok"
        if cmd == "jg":
            if self.mark == "-":
                return "er range nomark"
            self.start_move(int(self.mark))
            return "ok"
        if cmd == "k":
            return str(self.led)
        m = re.fullmatch(r"k(\d+)", cmd)
        if m:
            self.led = int(m.group(1))
            return "ok"
        if cmd == "b":
            self.start_move(self.pos, 1.0)
            return "ok"
        if cmd == "i":
            return self.name if self.lens else "er nolens"
        if cmd == "z":
            return self.climate
        if cmd == "x":
            if self.restarts:
                self.reset, ms = self.restarts.pop(0)
                self.boot = time.monotonic() - ms / 1000
            return "%s %d" % (self.reset, int((time.monotonic() - self.boot) * 1000))
        return "er nocap"


class Driver:
    """The driver under test, spoken to in INDI XML on its stdin, heard on its stdout."""

    def __init__(self, binary, home):
        self.stderr_path = os.path.join(home, "driver.stderr")
        self.proc = subprocess.Popen([binary], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                     stderr=open(self.stderr_path, "wb"),
                                     env={"HOME": home, "PATH": os.environ.get("PATH", "/usr/bin:/bin")})
        self.lock = threading.Lock()
        self.props = {}
        self.messages = []
        self.message_times = []     # when each message was read, from the driver's standard output
        self.updates = []
        threading.Thread(target=self.read, daemon=True).start()
        self.send("<getProperties version='1.7'/>")

    def read(self):
        parser = ET.XMLPullParser(events=("start", "end"))
        parser.feed("<indi>")
        depth = 0
        root = None
        pending = b""
        for data in iter(lambda: self.proc.stdout.read1(65536), b""):
            # The driver's XML declarations (`<?xml …?>`) don't belong inside the stream's root: taken out, a declaration
            # cut between two reads kept for the next one
            pending = re.sub(rb"<\?xml[^>]*\?>", b"", pending + data)
            cut = pending.rfind(b"<?")
            if cut >= 0 and b"?>" not in pending[cut:]:
                data, pending = pending[:cut], pending[cut:]
            else:
                data, pending = pending, b""
            parser.feed(data)
            for event, elem in parser.read_events():
                if event == "start":
                    if depth == 0:
                        root = elem
                    depth += 1
                    continue
                depth -= 1
                if depth == 1:
                    with self.lock:
                        self.take(elem)
                    root.clear()

    @staticmethod
    def text(elem):
        """The value INDI wrote: `\\n      <value>\\n  ` around it (indiuserio.c), the value itself kept whole."""
        t = elem.text or ""
        if t.startswith("\n      "):
            t = t[7:]
        if t.endswith("\n  "):
            t = t[:-3]
        return t

    def take(self, elem):
        tag = elem.tag
        if elem.get("message"):
            self.messages.append(elem.get("message"))
            self.message_times.append(time.monotonic())
        if tag == "message":
            return
        if tag == "delProperty":
            name = elem.get("name")
            for p in ([name] if name else list(self.props)):
                if p in self.props:
                    self.props[p]["defined"] = False
            return
        if not (tag.startswith("def") or tag.startswith("set")):
            return
        name = elem.get("name")
        p = self.props.setdefault(name, {"values": {}, "attrs": {}, "defined": False})
        if tag.startswith("def"):
            p["defined"] = True
        if elem.get("state"):
            p["state"] = elem.get("state")
        if elem.get("perm"):
            p["perm"] = elem.get("perm")
        for child in elem:
            value = self.text(child)
            if "Number" in tag:
                value = float(value.strip())
            elif "Switch" in tag:
                value = value.strip()
            p["values"][child.get("name")] = value
            attrs = {k: child.get(k) for k in ("min", "max", "format") if child.get(k) is not None}
            p["attrs"].setdefault(child.get("name"), {}).update(attrs)
        self.updates.append((time.monotonic(), name, p.get("state"), dict(p["values"])))

    def send(self, xml):
        self.proc.stdin.write(xml.encode() + b"\n")
        self.proc.stdin.flush()

    def new(self, kind, name, values):
        one = {"Number": "oneNumber", "Switch": "oneSwitch", "Text": "oneText"}[kind]
        body = "".join("<%s name='%s'>%s</%s>" % (one, k, v, one) for k, v in values.items())
        self.send("<new%sVector device='%s' name='%s'>%s</new%sVector>" % (kind, DEVICE, name, body, kind))

    def prop(self, name):
        with self.lock:
            p = self.props.get(name)
            return None if p is None else {"defined": p["defined"], "state": p.get("state"), "perm": p.get("perm"),
                                           "values": dict(p["values"]), "attrs": dict(p["attrs"])}

    def defined(self, name):
        p = self.prop(name)
        return p is not None and p["defined"]

    def value(self, name, element):
        p = self.prop(name)
        return None if p is None else p["values"].get(element)

    def state(self, name):
        p = self.prop(name)
        return None if p is None else p["state"]

    def logged(self, since_index=0):
        with self.lock:
            return list(self.messages[since_index:])

    def mark(self):
        with self.lock:
            return len(self.messages)

    def read_errors(self, since_index=0):
        """The `Serial read error` messages since since_index, each with the time it was read: (time, message)."""
        with self.lock:
            return [(t, x) for t, x in zip(self.message_times[since_index:], self.messages[since_index:])
                    if "Serial read error" in x]

    def wait(self, what, cond, timeout=10.0):
        end = time.monotonic() + timeout
        while time.monotonic() < end:
            if cond():
                return
            time.sleep(0.02)
        raise Failure("timed out waiting for %s" % what)

    def close(self):
        self.proc.kill()
        self.proc.wait()


class Failure(Exception):
    pass


def check(cond, what):
    if not cond:
        raise Failure(what)


class Session:
    """One driver process; each connection to a fresh fake controller on its own pty."""

    def __init__(self, binary, kind, home):
        self.kind = kind
        self.drv = Driver(binary, home)
        self.fake = None
        try:
            self.drv.wait("CONNECTION defined", lambda: self.drv.defined("CONNECTION"))
        except Failure:
            self.drv.close()
            raise

    def connect(self, basilisk=False, version="1.3", before=None):
        self.disconnect()
        self.fake = Fake(basilisk, version)
        if before:
            before(self.fake)
        self.drv.new("Text", "DEVICE_PORT", {"PORT": self.fake.port})
        self.drv.wait("DEVICE_PORT set", lambda: self.drv.value("DEVICE_PORT", "PORT") == self.fake.port)
        self.drv.new("Switch", "CONNECTION", {"CONNECT": "On", "DISCONNECT": "Off"})
        self.drv.wait("connection", lambda: self.drv.state("CONNECTION") == "Ok"
                      and self.drv.value("CONNECTION", "CONNECT") == "On"
                      and self.drv.defined("ABS_FOCUS_POSITION"), timeout=20)
        # The first refresh done: the connection's own reads are over
        self.drv.wait("first refresh", lambda: self.fake.commands().count("e") >= 1, timeout=5)
        return self.fake

    def disconnect(self):
        if self.fake is None:
            return
        if self.drv.value("CONNECTION", "CONNECT") == "On":
            self.drv.new("Switch", "CONNECTION", {"CONNECT": "Off", "DISCONNECT": "On"})
            self.drv.wait("disconnection", lambda: self.drv.value("CONNECTION", "DISCONNECT") == "On")
        time.sleep(0.2)
        self.fake.close()
        self.fake = None

    def barrier(self):
        """Every message sent to the driver before this one has been handled, and every message it wrote before
        handling this one has been read: FOCUS_SPEED is set, and its new value comes back."""
        value = 3 if self.drv.value("FOCUS_SPEED", "FOCUS_SPEED_VALUE") != 3 else 2
        self.drv.new("Number", "FOCUS_SPEED", {"FOCUS_SPEED_VALUE": value})
        self.drv.wait("FOCUS_SPEED at %d" % value, lambda: self.drv.value("FOCUS_SPEED", "FOCUS_SPEED_VALUE") == value)

    def rel(self, ticks):
        self.drv.new("Number", "REL_FOCUS_POSITION", {"FOCUS_RELATIVE_POSITION": ticks})

    def goto(self, target):
        self.drv.new("Number", "ABS_FOCUS_POSITION", {"FOCUS_ABSOLUTE_POSITION": target})

    def wait_still(self, name="ABS_FOCUS_POSITION", timeout=10):
        self.drv.wait("%s not Busy" % name, lambda: self.drv.state(name) not in (None, "Busy"), timeout)


# ── The Pinefeat suite: upstream's behaviour, and the defects of the fixes series ──

def pf_connect_versions(s):
    """Upstream's rule: `r` is read only when the minor of `v` is 3 or more. The controller connects whatever its
    version, and the extended set shows only as one `t` per connection (final)."""
    for version, reads_r in (("1.3", True), ("1.2", False), ("0.9", True), ("2.0", False)):
        fake = s.connect(version=version)
        cmds = fake.commands()
        check(("r" in cmds) == reads_r, "v=%s: r %s" % (version, "not read" if reads_r else "read"))
        check(cmds.count("t") == (1 if s.kind == "final" else 0), "v=%s: t sent %d times" % (version, cmds.count("t")))
        others = [c for c in cmds if EXT_LETTERS.match(c) and c != "t"]
        check(not others, "v=%s: letters of the extended set sent: %s" % (version, others))


def pf_driver_version(s):
    """The version the driver declares (DRIVER_INFO, from setVersion): upstream's 1.1, 1.2 after the fixes series
    (patch 0009), still 1.2 after the extended set, which doesn't change it (the human's decision, ticket #648)."""
    want = {"upstream": "1.1", "fixes": "1.2", "final": "1.2"}[s.kind]
    s.drv.wait("DRIVER_INFO", lambda: s.drv.value("DRIVER_INFO", "DRIVER_VERSION") is not None)
    got = s.drv.value("DRIVER_INFO", "DRIVER_VERSION")
    check(got == want, "driver version %r, not %r" % (got, want))


def pf_no_extended_set(s):
    """Without ext=1 no property of the extended set exists, and writing one by its name sends nothing: libindi 2
    refuses a property the driver has not defined before the driver sees it (`dispatch error: Property … is not
    defined`, on its standard error), as it does for upstream; the driver logs nothing."""
    fake = s.connect()
    for name in EXT_PROPERTIES:
        check(not s.drv.defined(name), "%s defined" % name)
    m = s.drv.mark()
    t0 = time.monotonic()
    s.drv.new("Number", "LED_BRIGHTNESS", {"LEVEL": 40})
    s.drv.new("Switch", "FOCUS_MARK_CTL", {"MARK_GOTO": "On"})
    s.drv.new("Switch", "LENS_REBOOT", {"REBOOT": "On"})
    s.drv.new("Switch", "FOCUS_ABORT_MOTION", {"ABORT": "On"})
    s.barrier()
    sent = [c for c in fake.commands(t0) if EXT_LETTERS.match(c)]
    check(not sent, "letters of the extended set sent: %s" % sent)
    with open(s.drv.stderr_path, "rb") as f:
        refused = f.read().decode(errors="replace")
    for name in ("LED_BRIGHTNESS", "FOCUS_MARK_CTL", "LENS_REBOOT", "FOCUS_ABORT_MOTION"):
        check("Property %s is not defined" % name in refused, "%s not refused by libindi: %r" % (name, refused))
    check(not s.drv.logged(m), "logged: %s" % s.drv.logged(m))


def pf_texts(s):
    """fix 1: the line end of the reply is not in the texts shown."""
    s.connect()
    for prop, element, want in (("FIRMWARE_VERSION", "FIRMWARE_VERSION", "1.3"),
                                ("FOCUS_DISTANCE", "FOCUS_DISTANCE", "1.25"),
                                ("RANGE_APERTURE", "APERTURE_RANGE", "1.8-22")):
        got = s.drv.value(prop, element)
        check(got == want, "%s is %r, not %r" % (prop, got, want))


def pf_nc_whole(s):
    """fix 1: only `nc` is "lens is not attached"; any other reply is logged as it came."""
    fake = s.connect()
    m = s.drv.mark()
    fake.script["f"] = "oops"
    s.drv.wait("the error logged", lambda: any("Can't read focus position" in x for x in s.drv.logged(m)))
    logged = [x for x in s.drv.logged(m) if "Can't read focus position" in x]
    check(any(x.endswith("Can't read focus position: oops.") for x in logged), "logged: %s" % logged)


def pf_goto(s):
    """A goto: `f<n>`, Busy, then Ok at the position once `e` answers n."""
    fake = s.connect()
    t0 = time.monotonic()
    s.goto(20000)
    s.drv.wait("Busy", lambda: s.drv.state("ABS_FOCUS_POSITION") == "Busy", 3)
    s.wait_still()
    check(s.drv.state("ABS_FOCUS_POSITION") == "Ok", "state %s" % s.drv.state("ABS_FOCUS_POSITION"))
    s.drv.wait("position 20000", lambda: s.drv.value("ABS_FOCUS_POSITION", "FOCUS_ABSOLUTE_POSITION") == 20000)
    check("f20000" in fake.commands(t0), "f20000 not sent")


def pf_relative_speed_aperture(s):
    """A relative move, the speed and the aperture: the letters upstream sends."""
    fake = s.connect()
    t0 = time.monotonic()
    s.drv.new("Switch", "FOCUS_MOTION", {"FOCUS_INWARD": "Off", "FOCUS_OUTWARD": "On"})
    s.drv.new("Number", "REL_FOCUS_POSITION", {"FOCUS_RELATIVE_POSITION": 100})
    s.wait_still("REL_FOCUS_POSITION")
    s.drv.new("Number", "FOCUS_SPEED", {"FOCUS_SPEED_VALUE": 2})
    s.drv.new("Number", "ABS_APERTURE", {"APERTURE_ABSOLUTE": 5.6})
    s.drv.wait("a5.6", lambda: "a5.6" in fake.commands(t0))
    cmds = fake.commands(t0)
    check("f+100" in cmds and "s2" in cmds, "sent: %s" % cmds)


def pf_reply_of_15(s):
    """fix 1: a reply of 15 characters is read whole and terminated (`er range limits`)."""
    s.connect()
    m = s.drv.mark()
    s.goto(30000)
    s.drv.wait("Alert", lambda: s.drv.state("ABS_FOCUS_POSITION") == "Alert", 5)
    logged = [x for x in s.drv.logged(m) if "Can't focus" in x]
    check(any(x.endswith("Can't focus: er range limits.") for x in logged), "logged: %s" % logged)


def pf_aperture_format(s):
    """fix 3: the aperture is shown with one decimal."""
    s.connect()
    for prop, element in (("ABS_APERTURE", "APERTURE_ABSOLUTE"), ("REL_APERTURE", "APERTURE_RELATIVE")):
        fmt = s.drv.prop(prop)["attrs"][element].get("format")
        check(fmt == "%.1f", "%s format %r" % (prop, fmt))


def pf_cadence(s):
    """fix 4: still, the state is read once a second: consecutive `e` at least a second apart (0.9 s, the margin of a
    clock read at different places), however slow the runner, which can only make them further apart."""
    fake = s.connect()
    t0 = time.monotonic()
    s.drv.wait("five `e`", lambda: len(fake.times("e", t0)) >= 5, 15)
    times = fake.times("e", t0)
    gap = min(b - a for a, b in zip(times, times[1:]))
    check(gap >= 0.9, "consecutive `e` %.3f s apart" % gap)


def pf_busy_ends(s):
    """fix 5: a move or a calibration Busy ends in Alert when the state can't be read once the focuser is still."""
    fake = s.connect()
    s.goto(20000)
    s.drv.wait("Busy", lambda: s.drv.state("ABS_FOCUS_POSITION") == "Busy", 3)
    with fake.lock:
        fake.lens = False
    s.drv.wait("Alert after the move", lambda: s.drv.state("ABS_FOCUS_POSITION") == "Alert", 6)
    with fake.lock:
        fake.lens = True
    s.drv.wait("Ok, the lens back", lambda: s.drv.state("ABS_FOCUS_POSITION") == "Ok", 6)
    s.drv.new("Switch", "CALIBRATE", {"CALIBRATE": "On"})
    s.drv.wait("calibration Busy", lambda: s.drv.state("FOCUS_MAX") == "Busy", 3)
    with fake.lock:
        fake.lens = False
    s.drv.wait("Alert after the calibration", lambda: s.drv.state("FOCUS_MAX") == "Alert"
               and s.drv.state("ABS_FOCUS_POSITION") == "Alert", 6)


def pf_log_once(s):
    """fix 6: without a lens, the position error is logged once, not at each refresh."""
    fake = s.connect()
    m = s.drv.mark()
    with fake.lock:
        fake.lens = False
    t0 = time.monotonic()
    s.drv.wait("four refreshes without a lens", lambda: len(fake.times("f", t0)) >= 4, 15)
    s.barrier()
    logged = [x for x in s.drv.logged(m) if "Can't read focus position" in x]
    check(len(logged) == 1, "error logged %d times over %d refreshes" % (len(logged), len(fake.times("f", t0))))
    check(logged[0].endswith("Can't read focus position: lens is not attached."), "logged: %s" % logged)


def pf_stale_line(s):
    """fix 8: a late reply partly received before a command: its rest is not that command's reply."""
    fake = s.connect()
    with fake.lock:
        fake.stale = ("a", b"LATE-PA", b"RT\n")
    s.drv.wait("the stale line", lambda: fake.stale_hit is not None and len(fake.commands()) > fake.stale_hit + 1, 5)
    # The command sent after the stale bytes, the next refresh's `e`: the rest of their line skipped, its reply is `n`,
    # and the refresh goes on with `f`
    cmds = fake.commands()[fake.stale_hit:fake.stale_hit + 2]
    check(cmds == ["e", "f"], "the command after the stale bytes, and the next one: %s" % cmds)


def bounded_errors(s, fake, m, count):
    """The command that started the endless input and the count - 1 after it each fail, their error logged within
    BOUND of the fake receiving them. Each failed command logs one `Serial read error` (sendCommand), in order: the
    i-th error since the mark m is the i-th command's."""
    t0 = fake.endless_from
    for i in range(count):
        s.drv.wait("command %d after the endless input began" % (i + 1), lambda: len(fake.received_from(t0)) > i, 10)
        tc, cmd = fake.received_from(t0)[i]
        s.drv.wait("the error of command %d (`%s`) within %.1f s" % (i + 1, cmd, BOUND),
                   lambda: len(s.drv.read_errors(m)) > i, max(0.0, tc + BOUND - time.monotonic()))
        te = s.drv.read_errors(m)[i][0]
        check(te - tc <= BOUND, "the error of command %d (`%s`) %.2f s after it" % (i + 1, cmd, te - tc))


def endless_then_back(s, fake, chunks, period, count):
    """After an `e`, the controller sends chunks for ever and answers nothing: the `e` and the commands after it fail
    within the bound (bounded_errors); once it stops, the refreshes go on: an `e` answered, then `f`."""
    m = s.drv.mark()
    with fake.lock:
        fake.endless_on = ("e", chunks, period)
    s.drv.wait("the endless input started", lambda: fake.endless_from is not None, 5)
    bounded_errors(s, fake, m, count)
    t1 = time.monotonic()
    fake.end_endless()
    s.drv.wait("the refreshes back: `f` sent", lambda: "f" in fake.commands(t1), 15)


def pf_endless_line(s):
    """A controller that sends bytes without a line end, fast and for ever: each command fails within the bound, and
    the driver goes on. Upstream bounds the reply in bytes (CEF_BUF), not in time, and a fast flow overflows it at once;
    the rest of a stale line (fix 8) and the stale input must be bounded too: this is the check of both."""
    fake = s.connect()
    endless_then_back(s, fake, FLOOD, 0.005, 3)


def pf_slow_line(s):
    """fix 7: a controller that sends a byte every 1.5 s without a line end, under the 3 s upstream waits for each byte
    and over CEF_TIMEOUT in all: each command fails within the bound."""
    fake = s.connect()
    endless_then_back(s, fake, [b"7"], 1.5, 2)


def pf_calibrate(s):
    """Upstream's CALIBRATE, kept against a Pinefeat controller (UPSTREAM.md, L09-03): `c`, Busy, then FOCUS_MAX
    takes the high bound of `r` once the focuser is still; the low bound is not used."""
    fake = s.connect()
    t0 = time.monotonic()
    s.drv.new("Switch", "CALIBRATE", {"CALIBRATE": "On"})
    s.drv.wait("FOCUS_MAX Busy", lambda: s.drv.state("FOCUS_MAX") == "Busy", 3)
    s.drv.wait("FOCUS_MAX at 25000", lambda: s.drv.state("FOCUS_MAX") != "Busy"
               and s.drv.value("FOCUS_MAX", "FOCUS_MAX_VALUE") == 25000, 8)
    check("c" in fake.commands(t0), "c not sent")
    check(float(s.drv.prop("ABS_FOCUS_POSITION")["attrs"]["FOCUS_ABSOLUTE_POSITION"]["min"]) == 0, "min moved")


def pf_version_form(s):
    """A reply to `v` that is not of the form `<major>.<minor>`: upstream, and the fixes, take it as the version; the
    extended set skips it as a line of the Basilisk board's ROM (patches/basilisk/PULL_REQUEST.md), and the controller
    is not detected. No Pinefeat controller is known to answer so: this fixes the difference, it does not judge it."""
    s.disconnect()
    s.fake = Fake(False, "abc")
    s.drv.new("Text", "DEVICE_PORT", {"PORT": s.fake.port})
    s.drv.wait("DEVICE_PORT set", lambda: s.drv.value("DEVICE_PORT", "PORT") == s.fake.port)
    m = s.drv.mark()
    s.drv.new("Switch", "CONNECTION", {"CONNECT": "On", "DISCONNECT": "Off"})
    if s.kind == "final":
        s.drv.wait("the connection refused", lambda: s.drv.state("CONNECTION") == "Alert"
                   or any("Can't detect the controller" in x for x in s.drv.logged(m)), 30)
        check(s.drv.value("CONNECTION", "CONNECT") != "On", "connected")
        check(s.fake.commands().count("v") >= 3, "v sent %d times" % s.fake.commands().count("v"))
    else:
        s.drv.wait("connection", lambda: s.drv.state("CONNECTION") == "Ok"
                   and s.drv.value("CONNECTION", "CONNECT") == "On", 20)
        check(s.drv.value("FIRMWARE_VERSION", "FIRMWARE_VERSION").startswith("abc"), "version %r"
              % s.drv.value("FIRMWARE_VERSION", "FIRMWARE_VERSION"))


# Each check: (function, the fix it shows or None, the symptom upstream gives: a fragment of the failure the check
# must report on upstream, or None for no deterministic symptom there, the check then not played on upstream).
PINEFEAT = [
    (pf_driver_version, None, ""),
    (pf_connect_versions, None, ""),
    (pf_no_extended_set, None, ""),
    (pf_goto, None, ""),
    (pf_relative_speed_aperture, None, ""),
    (pf_calibrate, None, ""),
    (pf_version_form, None, ""),
    (pf_texts, "1", "FIRMWARE_VERSION is '1.3\\n'"),
    (pf_nc_whole, "1", "Can't read focus position: lens is not attached."),
    (pf_reply_of_15, "1", None),
    (pf_aperture_format, "3", "format '%.f'"),
    (pf_cadence, "4", "consecutive `e` 0.0"),
    (pf_busy_ends, "5", "timed out waiting for Alert after the move"),
    (pf_log_once, "6", "error logged"),
    (pf_slow_line, "7", "timed out waiting for the error of command 1 (`e`)"),
    (pf_stale_line, "8", "and the next one: ['e', 'e']"),
    (pf_endless_line, None, ""),
]


# ── The Basilisk suite: the extended set, against a board that reports ext=1 ──

def bs_connect(s, **kw):
    fake = s.connect(basilisk=True, version="1.3", **kw)
    s.drv.wait("the extended set", lambda: all(s.drv.defined(p) for p in EXT_PROPERTIES), 5)
    return fake


def bs_restart(s):
    """Opening the port restarts the board: the ROM's lines come instead of the reply to the first `v`, and the log
    lines of a LOG ON left by the bench page come before every reply. The board is detected, ext=1 read."""
    def restarting(fake):
        fake.lose_v = 1
        fake.log_lines = True
    fake = bs_connect(s, before=restarting)
    check(s.drv.value("FIRMWARE_VERSION", "FIRMWARE_VERSION") == "1.3", "version %r"
          % s.drv.value("FIRMWARE_VERSION", "FIRMWARE_VERSION"))
    cmds = fake.commands()
    check(cmds.count("v") == 2 and cmds.count("t") == 1, "v %d, t %d" % (cmds.count("v"), cmds.count("t")))
    check("w" not in cmds and "n" not in cmds, "w or n sent")
    check(list(s.drv.prop("LENS_INFO")["values"]) == ["LENS_NAME"], "LENS_INFO %s" % s.drv.prop("LENS_INFO"))
    # a log line cut in two around a command: its rest is skipped, the refreshes go on
    with fake.lock:
        fake.stale = ("z", b"* tx 1C 00 ", b"01 02\n")
    s.drv.wait("the cut line", lambda: fake.stale_hit is not None and len(fake.commands()) > fake.stale_hit + 1, 5)
    cmds = fake.commands()[fake.stale_hit:fake.stale_hit + 2]
    check(cmds == ["e", "f"], "the command after the cut line, and the next one: %s" % cmds)
    check(s.drv.state("ABS_FOCUS_POSITION") == "Ok", "position %s" % s.drv.state("ABS_FOCUS_POSITION"))


def bs_bounds(s):
    """The bounds of `r` are the range of the position and of FOCUS_MAX; CALIBRATE reads them again, without `c`."""
    fake = bs_connect(s)
    attrs = s.drv.prop("ABS_FOCUS_POSITION")["attrs"]["FOCUS_ABSOLUTE_POSITION"]
    check((float(attrs["min"]), float(attrs["max"])) == (13873, 30738), "range %s" % attrs)
    check(s.drv.value("FOCUS_MAX", "FOCUS_MAX_VALUE") == 30738, "FOCUS_MAX %s"
          % s.drv.value("FOCUS_MAX", "FOCUS_MAX_VALUE"))
    t0 = time.monotonic()
    s.goto(5000)
    s.drv.wait("the goto refused", lambda: s.drv.state("ABS_FOCUS_POSITION") == "Alert", 3)
    s.barrier()
    check(not any(c.startswith("f5") for c in fake.commands(t0)), "f5000 sent: %s" % fake.commands(t0))
    with fake.lock:
        fake.limits = (1000, 9000)
    s.drv.new("Switch", "CALIBRATE", {"CALIBRATE": "On"})
    s.drv.wait("CALIBRATE Ok", lambda: s.drv.state("CALIBRATE") == "Ok", 3)
    s.drv.wait("the new bounds", lambda: s.drv.value("FOCUS_MAX", "FOCUS_MAX_VALUE") == 9000, 3)
    check("c" not in fake.commands(t0), "c sent")
    check(s.drv.state("ABS_FOCUS_POSITION") != "Busy", "Busy")
    with fake.lock:
        fake.lens = False
    s.drv.new("Switch", "CALIBRATE", {"CALIBRATE": "On"})
    s.drv.wait("CALIBRATE Alert", lambda: s.drv.state("CALIBRATE") == "Alert", 3)
    check(s.drv.value("FOCUS_MAX", "FOCUS_MAX_VALUE") == 9000, "bounds lost")


def bs_moves(s):
    """The end of a move read by `g`: ok, stall then unconfirmed, aborted by `q`."""
    fake = bs_connect(s)
    s.goto(20000)
    s.drv.wait("Busy", lambda: s.drv.state("ABS_FOCUS_POSITION") == "Busy", 3)
    s.wait_still()
    check(s.drv.state("ABS_FOCUS_POSITION") == "Ok", "ok: %s" % s.drv.state("ABS_FOCUS_POSITION"))
    with fake.lock:
        fake.next_result = "stall"
    m = s.drv.mark()
    s.goto(21000)
    s.drv.wait("Busy", lambda: s.drv.state("ABS_FOCUS_POSITION") == "Busy", 3)
    s.wait_still()
    check(s.drv.state("ABS_FOCUS_POSITION") == "Alert", "stall: %s" % s.drv.state("ABS_FOCUS_POSITION"))
    with fake.lock:
        fake.move_result = "unconfirmed"
    s.drv.wait("the stop not confirmed", lambda: any("Stop not confirmed" in x for x in s.drv.logged(m)), 4)
    t0 = time.monotonic()
    s.goto(25000)
    s.drv.wait("Busy", lambda: s.drv.state("ABS_FOCUS_POSITION") == "Busy", 3)
    s.drv.new("Switch", "FOCUS_ABORT_MOTION", {"ABORT": "On"})
    s.drv.wait("q", lambda: "q" in fake.commands(t0), 3)
    tq = fake.times("q", t0)[0]
    s.drv.wait("`g` read after `q`", lambda: len(fake.times("g", tq)) >= 1, 6)
    s.barrier()
    check(s.drv.state("ABS_FOCUS_POSITION") == "Idle", "aborted: %s" % s.drv.state("ABS_FOCUS_POSITION"))


def bs_relative(s):
    """A relative move, tracked to its end by `g` as an absolute one, on REL_FOCUS_POSITION too: `ok` Ok, `stall`
    Alert; aborted, Idle, then the stop not confirmed (`unconfirmed`) turns it to Alert."""
    fake = bs_connect(s)
    s.drv.new("Switch", "FOCUS_MOTION", {"FOCUS_INWARD": "Off", "FOCUS_OUTWARD": "On"})
    t0 = time.monotonic()
    s.rel(1000)
    s.drv.wait("Busy", lambda: s.drv.state("REL_FOCUS_POSITION") == "Busy", 3)
    s.wait_still("REL_FOCUS_POSITION")
    check(s.drv.state("REL_FOCUS_POSITION") == "Ok", "ok: %s" % s.drv.state("REL_FOCUS_POSITION"))
    check("f+1000" in fake.commands(t0) and "g" in fake.commands(t0), "sent: %s" % fake.commands(t0))
    s.drv.wait("position 17384", lambda: s.drv.value("ABS_FOCUS_POSITION", "FOCUS_ABSOLUTE_POSITION") == 17384, 3)
    with fake.lock:
        fake.next_result = "stall"
    s.rel(500)
    s.drv.wait("Busy", lambda: s.drv.state("REL_FOCUS_POSITION") == "Busy", 3)
    s.wait_still("REL_FOCUS_POSITION")
    check(s.drv.state("REL_FOCUS_POSITION") == "Alert", "stall: %s" % s.drv.state("REL_FOCUS_POSITION"))
    t0 = time.monotonic()
    s.rel(3000)
    s.drv.wait("Busy", lambda: s.drv.state("REL_FOCUS_POSITION") == "Busy", 3)
    s.drv.new("Switch", "FOCUS_ABORT_MOTION", {"ABORT": "On"})
    s.drv.wait("q", lambda: "q" in fake.commands(t0), 3)
    tq = fake.times("q", t0)[0]
    s.drv.wait("`g` read after `q`", lambda: len(fake.times("g", tq)) >= 1, 6)
    s.barrier()
    check(s.drv.state("REL_FOCUS_POSITION") == "Idle", "aborted: %s" % s.drv.state("REL_FOCUS_POSITION"))
    m = s.drv.mark()
    with fake.lock:
        fake.move_result = "unconfirmed"
    s.drv.wait("the stop not confirmed", lambda: s.drv.state("REL_FOCUS_POSITION") == "Alert"
               and any("Stop not confirmed" in x for x in s.drv.logged(m)), 5)


def bs_unreadable_ends(s):
    """A tracked move whose position `f` can't read at its end is Alert, though `g` says `ok`; a reboot (`b`) after
    which `f` doesn't answer is Alert too."""
    fake = bs_connect(s)
    t0 = time.monotonic()
    s.goto(20000)
    s.drv.wait("Busy", lambda: s.drv.state("ABS_FOCUS_POSITION") == "Busy", 3)
    with fake.lock:
        fake.lens = False
    s.wait_still()
    check(s.drv.state("ABS_FOCUS_POSITION") == "Alert", "move: %s" % s.drv.state("ABS_FOCUS_POSITION"))
    check("g" in fake.commands(t0), "g not read")
    with fake.lock:
        check(fake.move_result == "ok", "g is %s" % fake.move_result)
        fake.lens = True
    s.goto(21000)
    s.drv.wait("Busy", lambda: s.drv.state("ABS_FOCUS_POSITION") == "Busy", 3)
    s.wait_still()
    check(s.drv.state("ABS_FOCUS_POSITION") == "Ok", "lens back: %s" % s.drv.state("ABS_FOCUS_POSITION"))
    s.drv.new("Switch", "LENS_REBOOT", {"REBOOT": "On"})
    s.drv.wait("the reboot Busy", lambda: s.drv.state("ABS_FOCUS_POSITION") == "Busy"
               and s.drv.state("LENS_REBOOT") == "Ok", 3)
    with fake.lock:
        fake.lens = False
    s.wait_still()
    check(s.drv.state("ABS_FOCUS_POSITION") == "Alert", "reboot: %s" % s.drv.state("ABS_FOCUS_POSITION"))


def bs_led_config(s):
    """The LED level: the board keeps none (100 at each start). The driver saves it in its INDI config and sends it
    again at each connection: a new board, at 100, gets `k40`, and LEVEL reads 40 back from it."""
    fake = bs_connect(s)
    s.drv.new("Number", "LED_BRIGHTNESS", {"LEVEL": 40})
    s.drv.wait("k40", lambda: "k40" in fake.commands() and s.drv.value("LED_BRIGHTNESS", "LEVEL") == 40, 3)
    config = os.path.join(os.path.dirname(s.drv.stderr_path), ".indi", DEVICE + "_config.xml")
    s.drv.new("Switch", "CONFIG_PROCESS", {"CONFIG_SAVE": "On"})

    def saved():
        try:
            with open(config) as f:
                return "LED_BRIGHTNESS" in f.read()
        except OSError:
            return False
    s.drv.wait("the config saved with LED_BRIGHTNESS", saved, 5)
    fake = bs_connect(s)
    with fake.lock:
        check(fake.led == 40, "the new board's LED at %d" % fake.led)
    check("k40" in fake.commands(), "k40 not sent at the connection: %s" % fake.commands())
    s.drv.wait("LEVEL read back", lambda: s.drv.value("LED_BRIGHTNESS", "LEVEL") == 40, 3)


def bs_mark_led_reboot(s):
    """The mark (`j js jg jx`), the LED (`k`), the reboot (`b`)."""
    fake = bs_connect(s)
    check(s.drv.value("FOCUS_MARK", "MARK") == "16340", "mark %r" % s.drv.value("FOCUS_MARK", "MARK"))
    t0 = time.monotonic()
    s.drv.new("Switch", "FOCUS_MARK_CTL", {"MARK_SET": "On"})
    s.drv.wait("js", lambda: "js" in fake.commands(t0), 3)
    s.drv.wait("the mark", lambda: s.drv.value("FOCUS_MARK", "MARK") == "16384", 3)
    s.goto(20000)
    s.wait_still()
    s.drv.new("Switch", "FOCUS_MARK_CTL", {"MARK_GOTO": "On"})
    s.drv.wait("back at the mark", lambda: s.drv.value("ABS_FOCUS_POSITION", "FOCUS_ABSOLUTE_POSITION") == 16384
               and s.drv.state("ABS_FOCUS_POSITION") == "Ok", 6)
    s.drv.new("Switch", "FOCUS_MARK_CTL", {"MARK_CLEAR": "On"})
    s.drv.wait("the mark cleared", lambda: s.drv.value("FOCUS_MARK", "MARK") == "-", 3)
    s.drv.new("Number", "LED_BRIGHTNESS", {"LEVEL": 40})
    s.drv.wait("k40", lambda: "k40" in fake.commands(t0) and s.drv.value("LED_BRIGHTNESS", "LEVEL") == 40, 3)
    s.drv.new("Switch", "LENS_REBOOT", {"REBOOT": "On"})
    s.drv.wait("b", lambda: "b" in fake.commands(t0), 3)
    s.drv.wait("the reboot ended", lambda: s.drv.state("ABS_FOCUS_POSITION") == "Ok"
               and s.drv.state("LENS_REBOOT") == "Ok", 6)


def bs_read_back(s):
    """`o`, `i` and `z`, read once a second; `z` unknown deletes its properties."""
    fake = bs_connect(s)
    check(s.drv.value("APERTURE", "FNUMBER") == 5.6, "aperture %s" % s.drv.value("APERTURE", "FNUMBER"))
    check(s.drv.value("LENS_INFO", "LENS_NAME") == "SAMYANG AF 135mm F1.8", "name %r"
          % s.drv.value("LENS_INFO", "LENS_NAME"))
    for climate, want in (("21.37 48.02 9.91", (21.37, 48.02, 9.91)), ("-3.05 87.50 -4.27", (-3.05, 87.5, -4.27)),
                          ("-0.50 0.00 -", (-0.5, 0.0, None)), ("-", (None, None, None)),
                          ("21.37 48.02", (None, None, None)), ("21.37 48.02 9.91", (21.37, 48.02, 9.91))):
        with fake.lock:
            fake.climate = climate
        def seen():
            for (prop, element), value in zip((("FOCUS_TEMPERATURE", "TEMPERATURE"), ("FOCUS_HUMIDITY", "HUMIDITY"),
                                               ("FOCUS_DEW_POINT", "DEW_POINT")), want):
                if value is None and s.drv.defined(prop):
                    return False
                if value is not None and not (s.drv.defined(prop) and s.drv.value(prop, element) == value):
                    return False
            return True
        s.drv.wait("z %r" % climate, seen, 4)
    with fake.lock:
        fake.aperture = "er nocap ap"
        fake.name = "er nolens"
        fake.lens = False
    s.drv.wait("aperture Idle", lambda: s.drv.state("APERTURE") == "Idle", 4)
    s.drv.wait("name Alert, empty", lambda: s.drv.state("LENS_INFO") == "Alert"
               and s.drv.value("LENS_INFO", "LENS_NAME") == "", 4)


def bs_climate_moving(s):
    """`z` read while the focuser moves too: during a move of 10 s (`e` at y), a new value of `z` is published within
    2 s of the board reporting it, and `-` deletes the properties within 2 s, as when still; the move ends Ok."""
    fake = bs_connect(s)
    with fake.lock:
        fake.move_seconds = 10.0
    t0 = time.monotonic()
    s.goto(20000)
    s.drv.wait("the move started", lambda: "f20000" in fake.commands(t0), 5)
    climate = (("FOCUS_TEMPERATURE", "TEMPERATURE"), ("FOCUS_HUMIDITY", "HUMIDITY"), ("FOCUS_DEW_POINT", "DEW_POINT"))
    for value, want in (("18.25 61.40 10.72", (18.25, 61.4, 10.72)), ("-", (None, None, None))):
        with fake.lock:
            fake.climate = value
            tc = time.monotonic()
        def seen():
            return all(not s.drv.defined(p) if w is None else s.drv.defined(p) and s.drv.value(p, e) == w
                       for (p, e), w in zip(climate, want))
        s.drv.wait("z %r published within 2 s, while it moves" % value, seen, max(0.0, tc + 2.0 - time.monotonic()))
        check(time.monotonic() < fake.move_end and s.drv.state("ABS_FOCUS_POSITION") == "Busy",
              "z %r published after the move" % value)
    s.wait_still(timeout=15)
    check(s.drv.state("ABS_FOCUS_POSITION") == "Ok" and s.drv.value("ABS_FOCUS_POSITION", "FOCUS_ABSOLUTE_POSITION")
          == 20000, "the move's end: %s" % s.drv.prop("ABS_FOCUS_POSITION"))


# The warning of each cause that is not normal, written by hand (PROTOCOL.md, line `x`; the brief of ticket #694)
RESTART_WARNINGS = {"panic": "The board restarted: its firmware crashed (panic).",
                    "int_wdt": "The board restarted: its interrupt watchdog fired (int_wdt).",
                    "task_wdt": "The board restarted: its task watchdog fired (task_wdt).",
                    "wdt": "The board restarted: a hardware watchdog fired (wdt).",
                    "brownout": "The board restarted: its supply voltage dropped (brownout).",
                    "unknown": "The board restarted: its cause is unknown (unknown)."}
RESTART_NORMAL = ("poweron", "sw", "usb", "jtag")


def restart_warnings(s, since_index=0):
    """The warnings about a restart of the board since since_index, each with the time it was read: (time, message)."""
    with s.drv.lock:
        return [(t, x) for t, x in zip(s.drv.message_times[since_index:], s.drv.messages[since_index:])
                if "[WARNING]" in x and "The board restarted" in x]


def bs_restart_causes(s):
    """At the connection, BOARD_RESTART is the cause `x` reports, each of the ten of PROTOCOL.md; a cause that is not
    normal is logged as a warning that names it, Alert; poweron, sw, usb and jtag are not, Ok. BOARD_RESTART is read
    only, and the cause is read by `x`, never by `DEBUG` (PROTOCOL.md § 3: not for a driver)."""
    for cause in list(RESTART_WARNINGS) + list(RESTART_NORMAL):
        m = s.drv.mark()
        fake = bs_connect(s, before=lambda f: setattr(f, "reset", cause))
        check("x" in fake.commands(), "%s: x not sent at the connection" % cause)
        check(not [c for c in fake.commands() if c.upper().startswith("DEBUG")], "%s: DEBUG sent" % cause)
        check(s.drv.prop("BOARD_RESTART")["perm"] == "ro", "BOARD_RESTART perm %r" % s.drv.prop("BOARD_RESTART")["perm"])
        s.drv.wait("BOARD_RESTART %s" % cause, lambda: s.drv.value("BOARD_RESTART", "CAUSE") == cause, 3)
        if cause in RESTART_WARNINGS:
            s.drv.wait("the warning of %s" % cause, lambda: restart_warnings(s, m), 3)
            got = [x for _, x in restart_warnings(s, m)]
            check(len(got) == 1 and got[0].endswith(RESTART_WARNINGS[cause]), "%s: warned %s" % (cause, got))
            check(s.drv.state("BOARD_RESTART") == "Alert", "%s: %s" % (cause, s.drv.state("BOARD_RESTART")))
        else:
            s.barrier()
            check(not restart_warnings(s, m), "%s: warned %s" % (cause, restart_warnings(s, m)))
            check(s.drv.state("BOARD_RESTART") == "Ok", "%s: %s" % (cause, s.drv.state("BOARD_RESTART")))


def bs_reconnect(s):
    """A connection again to a board that has not restarted since the last one, still `panic`, its time grown (Ekos
    reconnecting): each connection reports it, a warning each time and BOARD_RESTART at panic, Alert, though the
    cause is the one the last connection read and the time did not go back (Handshake: restartKnown reset)."""
    boot = time.monotonic()
    for n in (1, 2):
        m = s.drv.mark()
        bs_connect(s, before=lambda f: (setattr(f, "reset", "panic"), setattr(f, "boot", boot - 3600 * n)))
        s.drv.wait("the warning of connection %d" % n, lambda: restart_warnings(s, m), 3)
        got = [x for _, x in restart_warnings(s, m)]
        check(len(got) == 1 and got[0].endswith(RESTART_WARNINGS["panic"]), "connection %d: warned %s" % (n, got))
        check(s.drv.value("BOARD_RESTART", "CAUSE") == "panic" and s.drv.state("BOARD_RESTART") == "Alert",
              "connection %d: BOARD_RESTART %r %s" % (n, s.drv.value("BOARD_RESTART", "CAUSE"),
                                                       s.drv.state("BOARD_RESTART")))


def bs_restart_live(s):
    """The board restarts while the driver stays connected, the port open, then resumes its lens (`e` at y for 4 s):
    within 3 s of the restart, while it resumes, BOARD_RESTART is the new cause and the warning is logged, once for
    that restart; a second restart of the same cause gives a second warning; seconds without a restart give none."""
    m = s.drv.mark()
    fake = bs_connect(s)
    t0 = time.monotonic()
    s.drv.wait("four `x` without a restart", lambda: len(fake.times("x", t0)) >= 4, 10)
    s.barrier()
    check(not restart_warnings(s, m), "warned without a restart: %s" % restart_warnings(s, m))
    check(s.drv.value("BOARD_RESTART", "CAUSE") == "poweron", "BOARD_RESTART %r" % s.drv.value("BOARD_RESTART", "CAUSE"))
    for n in (1, 2):
        tr = fake.restart("task_wdt", 4.0)
        s.drv.wait("warning %d within 3 s of restart %d" % (n, n), lambda: len(restart_warnings(s, m)) >= n,
                   max(0.0, tr + 3.0 - time.monotonic()))
        te, x = restart_warnings(s, m)[n - 1]
        # under 3 s, while the resume of 4 s goes on: no `f` sent since the restart, the driver saw `e` at y only
        check(te - tr < 3.0, "warning %d %.2f s after restart %d" % (n, te - tr, n))
        check(not [t for t in fake.times("f", tr) if t < te], "warning %d after the resume ended" % n)
        check(x.endswith(RESTART_WARNINGS["task_wdt"]), "warning %d: %s" % (n, x))
        check(s.drv.value("BOARD_RESTART", "CAUSE") == "task_wdt" and s.drv.state("BOARD_RESTART") == "Alert",
              "BOARD_RESTART %r %s" % (s.drv.value("BOARD_RESTART", "CAUSE"), s.drv.state("BOARD_RESTART")))
        # the resume over, then seconds more: still one warning for this restart
        s.drv.wait("the resume over", lambda: len(fake.times("f", tr)) >= 1, 10)
        tf = time.monotonic()
        s.drv.wait("three `x` after the resume", lambda: len(fake.times("x", tf)) >= 3, 10)
        s.barrier()
        check(len(restart_warnings(s, m)) == n, "%d warnings after %d restarts: %s"
              % (len(restart_warnings(s, m)), n, restart_warnings(s, m)))
    check(not [c for c in fake.commands() if c.upper().startswith("DEBUG")], "DEBUG sent")


def bs_restart_close(s):
    """Two restarts of the same cause a second apart: `x` reports `panic 300`, then, at the next read about a second
    later, `panic 900`. Its time did not go back, yet it is less than it would be without a restart: BOARD_RESTART is
    set again and the warning is logged, once for that restart."""
    m = s.drv.mark()
    fake = bs_connect(s)
    t0 = time.monotonic()
    s.drv.wait("an `x` while connected", lambda: fake.times("x", t0), 5)
    with fake.lock:
        fake.restarts = [("panic", 300), ("panic", 900)]
        t1 = time.monotonic()
    s.drv.wait("the two restarts read", lambda: len(fake.times("x", t1)) >= 2, 5)
    xa, xb = fake.times("x", t1)[:2]
    check(xb - xa < 1.5, "the two `x` %.2f s apart" % (xb - xa))
    s.drv.wait("the warning of the second restart", lambda: len(restart_warnings(s, m)) >= 2, 3)
    with s.drv.lock:
        updated = [t for t, name, _, _ in s.drv.updates if name == "BOARD_RESTART" and t > xb]
    check(updated, "BOARD_RESTART not set again")
    check(s.drv.value("BOARD_RESTART", "CAUSE") == "panic" and s.drv.state("BOARD_RESTART") == "Alert",
          "BOARD_RESTART %r %s" % (s.drv.value("BOARD_RESTART", "CAUSE"), s.drv.state("BOARD_RESTART")))
    t2 = time.monotonic()
    s.drv.wait("three `x` more", lambda: len(fake.times("x", t2)) >= 3, 10)
    s.barrier()
    got = [x for _, x in restart_warnings(s, m)]
    check(len(got) == 2 and all(x.endswith(RESTART_WARNINGS["panic"]) for x in got), "warned %s" % got)


def bs_no_false_restart(s):
    """A board that does not restart, its last restart a `panic` an hour ago, each reply delayed at random up to 0.4 s:
    over 30 s connected and the `x` read meanwhile, one warning only, the connection's."""
    m = s.drv.mark()
    def slow(fake):
        fake.reset = "panic"
        fake.boot = time.monotonic() - 3600
        fake.delay = 0.4
    fake = bs_connect(s, before=slow)
    s.drv.wait("the warning of the connection", lambda: restart_warnings(s, m), 5)
    t0 = time.monotonic()
    while time.monotonic() < t0 + 30:
        time.sleep(0.5)
        check(len(restart_warnings(s, m)) == 1, "warned without a restart: %s" % restart_warnings(s, m))
    s.barrier()
    n = len(fake.times("x", t0))
    check(n >= 5, "%d `x` in 30 s" % n)
    check(len(restart_warnings(s, m)) == 1, "warned without a restart: %s" % restart_warnings(s, m))


def bs_endless_lines(s):
    """The lines the extended set skips, without end: at `v`, lines that are not a version, for ever; after `e`, a
    line of the log (`* …`) that never ends. Each command fails within the bound: the board is not detected, then a
    board that answers is, and its refreshes go on once the endless line stops."""
    s.disconnect()
    s.fake = Fake(True, "1.3")
    with s.fake.lock:
        s.fake.endless_on = ("v", ROM_LINES, 0.01)
    s.drv.new("Text", "DEVICE_PORT", {"PORT": s.fake.port})
    s.drv.wait("DEVICE_PORT set", lambda: s.drv.value("DEVICE_PORT", "PORT") == s.fake.port)
    m = s.drv.mark()
    s.drv.new("Switch", "CONNECTION", {"CONNECT": "On", "DISCONNECT": "Off"})
    s.drv.wait("the endless lines started", lambda: s.fake.endless_from is not None, 5)
    bounded_errors(s, s.fake, m, 3)
    s.drv.wait("the board not detected", lambda: any("Can't detect the controller" in x for x in s.drv.logged(m)), 10)
    check(s.drv.value("CONNECTION", "CONNECT") != "On", "connected")
    fake = bs_connect(s)
    endless_then_back(s, fake, [b"* rx 05 len=65 ring=focus "] + FLOOD * 8, 0.005, 3)


def bs_back_to_pinefeat(s):
    """After the board, a Pinefeat controller on the same driver: no extended set, the range of upstream."""
    bs_connect(s)
    fake = s.connect(version="1.3")
    s.barrier()
    for name in EXT_PROPERTIES:
        check(not s.drv.defined(name), "%s defined" % name)
    attrs = s.drv.prop("ABS_FOCUS_POSITION")["attrs"]["FOCUS_ABSOLUTE_POSITION"]
    check((float(attrs["min"]), float(attrs["max"])) == (0, 32767), "range %s" % attrs)
    check(not [c for c in fake.commands() if EXT_LETTERS.match(c) and c != "t"], "extended letters sent")


BASILISK = [(bs_restart, None, ""), (bs_bounds, None, ""), (bs_moves, None, ""), (bs_relative, None, ""),
            (bs_unreadable_ends, None, ""), (bs_led_config, None, ""), (bs_mark_led_reboot, None, ""), (bs_read_back, None, ""),
            (bs_climate_moving, None, ""), (bs_restart_causes, None, ""), (bs_reconnect, None, ""),
            (bs_restart_live, None, ""), (bs_restart_close, None, ""), (bs_no_false_restart, None, ""),
            (bs_endless_lines, None, ""),
            (bs_back_to_pinefeat, None, "")]


def main():
    if len(sys.argv) != 4 or sys.argv[2] not in ("upstream", "fixes", "final") \
            or sys.argv[3] not in ("pinefeat", "basilisk"):
        sys.exit("usage: test_driver.py <driver binary> upstream|fixes|final pinefeat|basilisk")
    binary, kind, suite = sys.argv[1], sys.argv[2], sys.argv[3]
    if suite == "basilisk" and kind != "final":
        sys.exit("the basilisk suite is for the final driver")
    checks = PINEFEAT if suite == "pinefeat" else BASILISK
    failures = 0
    with tempfile.TemporaryDirectory() as home:
        for fn, fix, symptom in checks:
            if kind == "upstream" and symptom is None:
                print("skip  %-28s (fix %s: no deterministic symptom upstream)" % (fn.__name__, fix))
                continue
            expect_fail = kind == "upstream" and fix is not None
            s = None
            try:
                s = Session(binary, kind, home)
                fn(s)
                error = None
            except Failure as e:
                error = str(e)
            finally:
                if s is not None:
                    try:
                        s.disconnect()
                    except Failure:
                        pass
                    s.drv.close()
                    if s.fake is not None:
                        s.fake.close()
            label = "%-28s %s" % (fn.__name__, "(fix %s)" % fix if fix else "")
            if expect_fail:
                # The defect the fix names, not any failure: the check's failure carries its symptom
                ok = error is not None and symptom in error
                seen = "no defect seen" if error is None else ("defect shown: " if ok else "not the defect named (%r): "
                                                               % symptom) + error
                print("%s  %s — upstream: %s" % ("ok  " if ok else "FAIL", label, seen))
            else:
                ok = error is None
                print("%s  %s%s" % ("ok  " if ok else "FAIL", label, "" if ok else " — " + error))
            failures += not ok
            if not ok:
                with open(os.path.join(home, "driver.stderr"), "rb") as f:
                    tail = f.read()[-2000:].decode(errors="replace")
                if tail:
                    print("    driver stderr: " + tail.replace("\n", "\n    "))
    print("%s %s: %s" % (kind, suite, "%d failure(s)" % failures if failures else "all checks pass"))
    sys.exit(1 if failures else 0)


if __name__ == "__main__":
    main()
