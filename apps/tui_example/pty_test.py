"""Runs the terminal example in a pseudo terminal and checks what a real terminal sees.

The parent keeps the slave end open so it can read the terminal attributes the application leaves
behind, which is how restoring raw mode is verified.
"""

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
RESET_SIGNALS = (signal.SIGHUP, signal.SIGINT, signal.SIGTERM, signal.SIGQUIT, signal.SIGPIPE)


def prepare_child():
    # The test runner may ignore or catch these, and the application only installs handlers for signals that
    # are not ignored.
    for number in RESET_SIGNALS:
        signal.signal(number, signal.SIG_DFL)


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
        self.process = subprocess.Popen(
            [BINARY, *args],
            stdin=self.slave,
            stdout=subprocess.PIPE if stdout_to_pipe else self.slave,
            stderr=self.slave,
            env=env,
            start_new_session=True,
            preexec_fn=prepare_child,
            close_fds=True,
        )
        self.output = ""
        self._pending = b""

    def close(self):
        if self.process.poll() is None:
            self.process.kill()
            self.process.wait()
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
        self._pending += chunk
        # Keep a split multi-byte character for the next read.
        try:
            self.output += self._pending.decode("utf-8")
            self._pending = b""
        except UnicodeDecodeError as error:
            if error.start == 0 and len(self._pending) < 4:
                return True
            self.output += self._pending[: error.start].decode("utf-8")
            self._pending = self._pending[error.start :]
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
            if self.process.poll() is not None:
                # Drain what the application wrote before it exited, then give up instead of waiting out the timeout.
                while self._read_available(0.1):
                    pass
                if needle in text():
                    break
                self.test.fail(
                    "process exited with %r while waiting for %r; output so far: %r"
                    % (self.process.returncode, needle, self.output[-600:])
                )
            self._read_available(min(remaining, 0.2))
        return len(self.output)

    def send(self, data):
        os.write(self.master, data.encode("utf-8"))

    def lflag(self):
        return termios.tcgetattr(self.slave)[3]

    def wait_until_quiet(self, quiet=0.6):
        """Waits until no output arrives for `quiet` seconds, i.e. the application stopped drawing."""
        last_change = time.monotonic()
        deadline = last_change + TIMEOUT
        size = len(self.output)
        while time.monotonic() - last_change < quiet:
            if time.monotonic() > deadline:
                self.test.fail("output kept changing; output so far: %r" % self.output[-300:])
            self._read_available(0.05)
            if len(self.output) != size:
                size = len(self.output)
                last_change = time.monotonic()

    def wait_exit(self):
        # The output has to be drained while waiting: a full pty buffer blocks the application's writes.
        deadline = time.monotonic() + TIMEOUT
        while self.process.poll() is None:
            if time.monotonic() > deadline:
                self.test.fail("process did not exit; output so far: %r" % self.output[-600:])
            self._read_available(0.02)
        self.exited_at = time.monotonic()
        while self._read_available(0.3):
            pass
        return self.process.returncode

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
        os.kill(session.process.pid, signal.SIGWINCH)
        session.wait_for("120x40", mark)

        session.send("\x1b")
        self.assertEqual(session.wait_exit(), 0)
        session.assert_terminal_restored()

    def test_sigterm_restores_terminal(self):
        session = self.start()
        session.wait_for(HIDE_CURSOR)
        self.assertEqual(session.lflag() & COOKED, 0)

        session.process.send_signal(signal.SIGTERM)
        self.assertEqual(session.wait_exit(), -signal.SIGTERM)
        session.assert_terminal_restored(ends_with_show_cursor=True)

    def test_sighup_restores_terminal(self):
        session = self.start()
        session.wait_for(HIDE_CURSOR)
        session.process.send_signal(signal.SIGHUP)
        self.assertEqual(session.wait_exit(), -signal.SIGHUP)
        session.assert_terminal_restored(ends_with_show_cursor=True)

    def test_ctrl_c_exits_with_130_and_restores_terminal(self):
        session = self.start()
        session.wait_for(HIDE_CURSOR)
        self.assertEqual(session.lflag() & COOKED, 0)

        session.send("\x03")
        self.assertEqual(session.wait_exit(), 130)
        session.assert_terminal_restored()

    def test_ctrl_c_exits_when_javascript_is_hung(self):
        session = self.start("--hang")
        session.wait_for(HIDE_CURSOR)
        # The spinner redraws every 120 ms until the JS thread blocks, so silence means the hang engaged.
        session.wait_until_quiet()

        started = time.monotonic()
        session.send("\x03")
        self.assertEqual(session.wait_exit(), 130)
        self.assertLess(time.monotonic() - started, 5.0)
        session.assert_terminal_restored()

    def test_second_ctrl_c_does_not_wait_for_the_watchdog(self):
        session = self.start("--hang")
        session.wait_for(HIDE_CURSOR)
        session.wait_until_quiet()

        started = time.monotonic()
        session.send("\x03")
        time.sleep(0.1)
        # The first Ctrl+C already restored the cooked terminal, so a real terminal turns the second one into a
        # SIGINT. The pty is not this process's controlling terminal here, so send that signal directly; the host's
        # handler restores the terminal and dies by it, which a shell reports as 130 too.
        session.process.send_signal(signal.SIGINT)
        self.assertIn(session.wait_exit(), (130, -signal.SIGINT))
        # The watchdog gives the application a full second before it forces the exit.
        self.assertLess(session.exited_at - started, 0.8)
        session.assert_terminal_restored()

    def test_crash_signal_restores_terminal_and_prints_a_trace(self):
        session = self.start()
        session.wait_for(HIDE_CURSOR)
        self.assertEqual(session.lflag() & COOKED, 0)

        session.process.send_signal(signal.SIGSEGV)
        self.assertEqual(session.wait_exit(), -signal.SIGSEGV)
        session.assert_terminal_restored()
        self.assertIn("Stack trace", session.output)

    def test_abort_restores_terminal(self):
        session = self.start()
        session.wait_for(HIDE_CURSOR)
        session.process.send_signal(signal.SIGABRT)
        self.assertEqual(session.wait_exit(), -signal.SIGABRT)
        session.assert_terminal_restored()

    def test_redirected_stdout_leaves_the_terminal_alone(self):
        session = self.start(stdout_to_pipe=True)
        # Nothing is drawn on the pipe, so there is no frame to wait for; give the application time to start.
        time.sleep(1.5)
        self.assertEqual(terminal_modes(session.slave), session.initial_modes, "raw mode with stdout redirected")

        session.process.send_signal(signal.SIGTERM)
        self.assertEqual(session.wait_exit(), -signal.SIGTERM)
        self.assertEqual(terminal_modes(session.slave), session.initial_modes)


if __name__ == "__main__":
    unittest.main(argv=[sys.argv[0]])
