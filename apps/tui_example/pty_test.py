"""Runs the terminal example in a pseudo terminal and checks what a real terminal sees.

The parent keeps the slave end open so it can read the terminal attributes the application leaves
behind, which is how restoring raw mode is verified. Like a shell, a small session leader owns the
pty as its controlling terminal and runs the application as the foreground job; it outlives the
application, because macOS resets a terminal whose session leader exits.
"""

import codecs
import fcntl
import os
import pty
import re
import select
import signal
import struct
import subprocess
import sys
import termios
import time
import unittest

BINARY = sys.argv.pop(1)

TIMEOUT = 30.0
HIDE_CURSOR = "\x1b[?25l"
SHOW_CURSOR = "\x1b[?25h"
COOKED = termios.ECHO | termios.ICANON
PROMPT_CURSOR = "\u258c"
PROMPT_BOX_END = "\u256f"
# Lines the example logs with --report-ready and --hang; see index.tsx.
READY_MARKER = "tui_example: ready"
HUNG_MARKER = "tui_example: blocking the JS thread"
# The host's grace period between the first Ctrl+C and the forced exit (kExitGraceMs in TerminalHost.cpp).
EXIT_GRACE = 1.0


# Runs as the session leader (start_new_session) with the pty on stdin, stdout and stderr. Reports the
# application's pid and then its exit status (in Popen.returncode form) on the status pipe, and stays alive
# until the test closes the release pipe.
SESSION_LEADER = r"""
import fcntl, os, signal, sys, termios
status_fd, release_fd = int(sys.argv[1]), int(sys.argv[2])
fcntl.ioctl(0, termios.TIOCSCTTY, 0)
go_read, go_write = os.pipe()
pid = os.fork()
if pid == 0:
    os.close(go_write)
    os.read(go_read, 1)
    # The test runner may ignore or catch these, and the application only installs handlers for signals that
    # are not ignored.
    for number in (signal.SIGHUP, signal.SIGINT, signal.SIGTERM, signal.SIGQUIT, signal.SIGPIPE):
        signal.signal(number, signal.SIG_DFL)
    os.closerange(3, 256)
    os.execv(sys.argv[3], sys.argv[3:])
os.close(go_read)
for number in (signal.SIGHUP, signal.SIGINT, signal.SIGQUIT, signal.SIGTSTP, signal.SIGTTOU, signal.SIGTTIN):
    signal.signal(number, signal.SIG_IGN)
os.setpgid(pid, pid)
os.tcsetpgrp(0, pid)
os.close(go_write)
os.write(status_fd, b"%d\n" % pid)
_, status = os.waitpid(pid, 0)
os.write(status_fd, b"%d\n" % os.waitstatus_to_exitcode(status))
os.read(release_fd, 1)
"""


def terminal_modes(fd):
    """Everything raw mode touches, so a partial restore is caught as well as a missing one."""
    attributes = termios.tcgetattr(fd)
    iflag, oflag, cflag, lflag = attributes[:4]
    control = attributes[6]
    return (iflag, oflag, cflag, lflag, control[termios.VMIN], control[termios.VTIME])
ANSI = re.compile(r"\x1b\[[0-9;?]*[A-Za-z]")


class PtySession:
    def __init__(self, test, args=(), rows=30, cols=100, stdout_to_pipe=False):
        self.test = test
        self.master, self.slave = pty.openpty()
        fcntl.ioctl(self.slave, termios.TIOCSWINSZ, struct.pack("HHHH", rows, cols, 0, 0))
        self.initial_modes = terminal_modes(self.slave)
        env = dict(os.environ, TERM="xterm-256color")
        env.pop("NO_COLOR", None)
        status_read, status_write = os.pipe()
        release_read, self._release = os.pipe()
        self.process = subprocess.Popen(
            [sys.executable, "-c", SESSION_LEADER, str(status_write), str(release_read), BINARY, *args],
            stdin=self.slave,
            stdout=subprocess.PIPE if stdout_to_pipe else self.slave,
            stderr=self.slave,
            env=env,
            start_new_session=True,
            pass_fds=(status_write, release_read),
        )
        os.close(status_write)
        os.close(release_read)
        self._status = os.fdopen(status_read, "rb")
        self.pid = int(self._status.readline())
        os.set_blocking(status_read, False)
        self._status_buffer = b""
        self.returncode = None
        self.output = ""
        self._decoder = codecs.getincrementaldecoder("utf-8")(errors="replace")

    def poll(self):
        """The application's exit status in Popen.returncode form, or None while it runs."""
        if self.returncode is None:
            chunk = self._status.read()
            if chunk:
                self._status_buffer += chunk
            if self._status_buffer.endswith(b"\n"):
                self.returncode = int(self._status_buffer)
        return self.returncode

    def signal(self, number):
        os.kill(self.pid, number)

    def close(self):
        if self.poll() is None:
            os.kill(self.pid, signal.SIGKILL)
        os.close(self._release)
        self.process.wait()
        self._status.close()
        if self.process.stdout is not None:
            self.process.stdout.close()
        os.close(self.master)
        os.close(self.slave)

    def _read_available(self, timeout):
        ready, _, _ = select.select([self.master], [], [], timeout)
        if not ready:
            return False
        try:
            chunk = os.read(self.master, 65536)
        except OSError:
            return False
        if not chunk:
            # The terminal was hung up: nothing more will arrive.
            return False
        # Keeps a character split across reads for the next one. A signal can cut a frame write mid-character, so
        # invalid bytes are replaced instead of stalling the decoder.
        self.output += self._decoder.decode(chunk)
        return True

    def wait_for(self, needle, start=0, plain=False):
        """Waits until `needle` appears after offset `start`; `plain` ignores the ANSI styling in between."""

        def text():
            tail = self.output[start:]
            return ANSI.sub("", tail) if plain else tail

        deadline = time.monotonic() + TIMEOUT
        while needle not in text():
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                self.test.fail("timed out waiting for %r; output so far: %r" % (needle, self.output[-600:]))
            if self.poll() is not None:
                # Drain what the application wrote before it exited, then give up instead of waiting out the timeout.
                while self._read_available(0.1):
                    pass
                if needle in text():
                    break
                self.test.fail(
                    "process exited with %r while waiting for %r; output so far: %r"
                    % (self.returncode, needle, self.output[-600:])
                )
            self._read_available(min(remaining, 0.2))
        return len(self.output)

    def wait_for_stdout_pipe(self, needle):
        """Like wait_for, for a session started with stdout_to_pipe: reads the pipe instead of the terminal."""
        fd = self.process.stdout.fileno()
        received = b""
        deadline = time.monotonic() + TIMEOUT
        while needle.encode("utf-8") not in received:
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                self.test.fail("timed out waiting for %r on stdout; got %r" % (needle, received[-600:]))
            ready, _, _ = select.select([fd], [], [], min(remaining, 0.2))
            if ready:
                chunk = os.read(fd, 65536)
                if not chunk:
                    self.test.fail("stdout closed while waiting for %r; got %r" % (needle, received[-600:]))
                received += chunk

    def send(self, data):
        os.write(self.master, data.encode("utf-8"))

    def lflag(self):
        return termios.tcgetattr(self.slave)[3]

    def wait_for_frame_end(self, start=0):
        """Waits until the frame drawn after offset `start` is complete: the prompt box with its cursor closes it."""
        self.wait_for(PROMPT_CURSOR, start)
        self.wait_for(PROMPT_BOX_END, self.output.find(PROMPT_CURSOR, start))

    def wait_for_first_frame(self):
        """Waits until a whole frame is on screen, so the application is not blocked writing to a full pty."""
        self.wait_for(HIDE_CURSOR)
        self.wait_for_frame_end()

    def wait_for_hung_frame(self):
        """Waits for the --hang marker and the rest of the frame painted with it, after which nothing is drawn."""
        self.wait_for(HIDE_CURSOR)
        self.wait_for(HUNG_MARKER)
        self.wait_for_frame_end(self.output.find(HUNG_MARKER))

    def wait_for_stopped(self):
        """Waits until the application is stopped by job control."""
        deadline = time.monotonic() + TIMEOUT
        while True:
            state = subprocess.run(
                ["ps", "-o", "state=", "-p", str(self.pid)], capture_output=True, text=True
            ).stdout.strip()
            if state.startswith("T"):
                return
            if time.monotonic() > deadline:
                self.test.fail("the application never stopped (state %r); output so far: %r" % (state, self.output[-300:]))
            if self.poll() is not None:
                self.test.fail("the application exited with %r instead of stopping" % self.returncode)
            self._read_available(0.02)

    def wait_for_cooked_mode(self):
        """Waits until the terminal is back in cooked mode, which the host does first when it starts exiting."""
        deadline = time.monotonic() + TIMEOUT
        while self.lflag() & COOKED != COOKED:
            if time.monotonic() > deadline:
                self.test.fail("the terminal never left raw mode; output so far: %r" % self.output[-300:])
            if self.poll() is not None:
                return
            self._read_available(0.01)

    def wait_exit(self):
        # The output has to be drained while waiting: a full pty buffer blocks the application's writes.
        deadline = time.monotonic() + TIMEOUT
        while self.poll() is None:
            if time.monotonic() > deadline:
                self.test.fail("process did not exit; output so far: %r" % self.output[-600:])
            self._read_available(0.02)
        self.exited_at = time.monotonic()
        while self._read_available(0.3):
            pass
        return self.returncode

    def fill_output_buffer(self):
        """Stops draining the master until the application's writes block on the full pty buffer."""
        deadline = time.monotonic() + TIMEOUT
        while True:
            # A pty that accepts no more writes is full; the application's own frame write is then blocked too.
            try:
                fd = os.open(os.ttyname(self.slave), os.O_WRONLY | os.O_NONBLOCK | os.O_NOCTTY)
            except OSError:
                fd = None
            if fd is not None:
                try:
                    os.write(fd, b"")
                    ready = select.select([], [fd], [], 0)[1]
                finally:
                    os.close(fd)
                if not ready:
                    # Give the application time to block in write(2) on the remainder of its frame.
                    time.sleep(0.3)
                    return
            if time.monotonic() > deadline:
                self.test.fail("the pty output buffer never filled")
            time.sleep(0.05)

    def last_cursor_visibility(self):
        show = self.output.rfind(SHOW_CURSOR)
        hide = self.output.rfind(HIDE_CURSOR)
        return None if show < 0 and hide < 0 else (SHOW_CURSOR if show > hide else HIDE_CURSOR)

    def assert_terminal_restored(self, ends_with_show_cursor=False):
        self.test.assertEqual(terminal_modes(self.slave), self.initial_modes, "terminal modes not restored")
        self.test.assertGreater(
            self.output.rfind(SHOW_CURSOR),
            self.output.rfind(HIDE_CURSOR),
            "the cursor must be visible again, got %r" % self.output[-120:],
        )
        if ends_with_show_cursor:
            # A signal kills the process right after the restore, so nothing follows it.
            self.test.assertTrue(
                self.output.rstrip("\r\n").endswith(SHOW_CURSOR),
                "output must end by showing the cursor, got %r" % self.output[-80:],
            )


class TuiExamplePtyTest(unittest.TestCase):
    def start(self, *args, **kwargs):
        session = PtySession(self, args, **kwargs)
        self.addCleanup(session.close)
        return session

    def test_raw_mode_input_resize_and_escape(self):
        session = self.start()
        session.wait_for(HIDE_CURSOR)
        session.wait_for("Valdi TUI example")
        session.wait_for("100x30")
        self.assertEqual(session.lflag() & COOKED, 0, "ECHO and ICANON must be off in raw mode")

        mark = len(session.output)
        session.send("\x1b[B")
        session.wait_for("Turns TypeScript", mark)

        mark = len(session.output)
        session.send("hi")
        session.wait_for("> hi", mark, plain=True)

        fcntl.ioctl(session.slave, termios.TIOCSWINSZ, struct.pack("HHHH", 40, 120, 0, 0))
        mark = len(session.output)
        session.signal(signal.SIGWINCH)
        session.wait_for("120x40", mark)

        session.send("\x1b")
        self.assertEqual(session.wait_exit(), 0)
        session.assert_terminal_restored()

    def test_sigterm_restores_terminal(self):
        session = self.start()
        session.wait_for_first_frame()
        self.assertEqual(session.lflag() & COOKED, 0)

        session.signal(signal.SIGTERM)
        self.assertEqual(session.wait_exit(), -signal.SIGTERM)
        session.assert_terminal_restored(ends_with_show_cursor=True)

    def test_signal_during_a_blocked_frame_write_restores_terminal(self):
        for number in (signal.SIGTERM, signal.SIGHUP):
            with self.subTest(signal=number):
                session = PtySession(self)
                try:
                    session.wait_for(HIDE_CURSOR)
                    session.fill_output_buffer()
                    self.assertIsNone(session.poll())
                    session.signal(number)
                    self.assertEqual(session.wait_exit(), -number)
                    self.assertEqual(terminal_modes(session.slave), session.initial_modes)
                    self.assertEqual(
                        session.last_cursor_visibility(), SHOW_CURSOR, "cursor left hidden: %r" % session.output[-120:]
                    )
                finally:
                    session.close()

    def test_ctrl_c_during_a_blocked_frame_write_restores_terminal(self):
        session = self.start()
        session.wait_for_first_frame()
        session.fill_output_buffer()
        session.send("\x03")
        self.assertEqual(session.wait_exit(), 130)
        self.assertEqual(terminal_modes(session.slave), session.initial_modes)
        self.assertEqual(session.last_cursor_visibility(), SHOW_CURSOR, "cursor left hidden: %r" % session.output[-120:])

    def test_sighup_restores_terminal(self):
        session = self.start()
        session.wait_for_first_frame()
        session.signal(signal.SIGHUP)
        self.assertEqual(session.wait_exit(), -signal.SIGHUP)
        session.assert_terminal_restored(ends_with_show_cursor=True)

    def test_ctrl_c_exits_with_130_and_restores_terminal(self):
        session = self.start()
        session.wait_for_first_frame()
        self.assertEqual(session.lflag() & COOKED, 0)

        session.send("\x03")
        self.assertEqual(session.wait_exit(), 130)
        session.assert_terminal_restored()

    def test_ctrl_c_exits_when_javascript_is_hung(self):
        session = self.start("--hang")
        # Logged from the task that then loops forever, so from here on the JS thread cannot run an exit.
        session.wait_for_hung_frame()

        started = time.monotonic()
        session.send("\x03")
        self.assertEqual(session.wait_exit(), 130)
        elapsed = session.exited_at - started
        # Only the watchdog can end a process whose JS is stuck, and it waits out the full grace period.
        self.assertGreaterEqual(elapsed, EXIT_GRACE * 0.9)
        self.assertLess(elapsed, EXIT_GRACE + 4.0)
        session.assert_terminal_restored()

    def test_second_ctrl_c_does_not_wait_for_the_watchdog(self):
        session = self.start("--hang")
        session.wait_for_hung_frame()

        mark = len(session.output)
        started = time.monotonic()
        session.send("\x03")
        # The host restores the terminal (cooked modes, then the cursor) when it handles the first Ctrl+C, before it
        # starts the watchdog.
        session.wait_for_cooked_mode()
        session.wait_for(SHOW_CURSOR, mark)
        self.assertIsNone(session.poll(), "the hung application must still be waiting for the watchdog")
        # With the cooked modes back, the terminal turns this Ctrl+C into a SIGINT; the host's handler restores the
        # terminal and dies by it, which a shell reports as 130 too.
        session.send("\x03")
        self.assertIn(session.wait_exit(), (130, -signal.SIGINT))
        # JS is stuck, so nothing but the watchdog or the second Ctrl+C can end the process, and the watchdog waits
        # out the full grace period.
        self.assertLess(session.exited_at - started, EXIT_GRACE * 0.8)
        session.assert_terminal_restored()

    def test_sigtstp_hands_back_the_terminal_and_sigcont_redraws(self):
        session = self.start()
        session.wait_for_first_frame()
        self.assertEqual(session.lflag() & COOKED, 0)

        mark = len(session.output)
        session.signal(signal.SIGTSTP)
        session.wait_for_stopped()
        session.wait_for(SHOW_CURSOR, mark)
        self.assertEqual(terminal_modes(session.slave), session.initial_modes, "cooked modes not handed back")

        mark = len(session.output)
        session.signal(signal.SIGCONT)
        session.wait_for(HIDE_CURSOR, mark)
        session.wait_for("Valdi TUI example", mark)
        session.wait_for_frame_end(mark)
        self.assertEqual(session.lflag() & COOKED, 0, "raw mode must be back after SIGCONT")

        mark = len(session.output)
        session.send("hi")
        session.wait_for("> hi", mark, plain=True)

        session.send("\x1b")
        self.assertEqual(session.wait_exit(), 0)
        session.assert_terminal_restored()

    def test_crash_signal_restores_terminal_and_prints_a_trace(self):
        session = self.start()
        session.wait_for_first_frame()
        self.assertEqual(session.lflag() & COOKED, 0)

        session.signal(signal.SIGSEGV)
        self.assertEqual(session.wait_exit(), -signal.SIGSEGV)
        session.assert_terminal_restored()
        self.assertIn("Stack trace", session.output)

    def test_abort_restores_terminal(self):
        session = self.start()
        session.wait_for_first_frame()
        session.signal(signal.SIGABRT)
        self.assertEqual(session.wait_exit(), -signal.SIGABRT)
        session.assert_terminal_restored()

    def test_redirected_stdout_leaves_the_terminal_alone(self):
        session = self.start("--report-ready", stdout_to_pipe=True)
        # The application logs this once its first frame is out, after it decided against raw mode.
        session.wait_for_stdout_pipe(READY_MARKER)
        self.assertEqual(terminal_modes(session.slave), session.initial_modes, "raw mode with stdout redirected")

        session.signal(signal.SIGTERM)
        self.assertEqual(session.wait_exit(), -signal.SIGTERM)
        self.assertEqual(terminal_modes(session.slave), session.initial_modes)


if __name__ == "__main__":
    unittest.main(argv=[sys.argv[0]])
