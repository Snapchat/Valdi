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

TIMEOUT = 10.0
HIDE_CURSOR = "\x1b[?25l"
SHOW_CURSOR = "\x1b[?25h"
COOKED = termios.ECHO | termios.ICANON
ANSI = re.compile(r"\x1b\[[0-9;?]*[A-Za-z]")


class PtySession:
    def __init__(self, test, args=(), rows=30, cols=100):
        self.test = test
        self.master, self.slave = pty.openpty()
        fcntl.ioctl(self.slave, termios.TIOCSWINSZ, struct.pack("HHHH", rows, cols, 0, 0))
        self.initial_lflag = termios.tcgetattr(self.slave)[3]
        env = dict(os.environ, TERM="xterm-256color")
        env.pop("NO_COLOR", None)
        self.process = subprocess.Popen(
            [BINARY, *args],
            stdin=self.slave,
            stdout=self.slave,
            stderr=self.slave,
            env=env,
            preexec_fn=os.setsid,
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
            self._read_available(min(remaining, 0.2))
        return len(self.output)

    def send(self, data):
        os.write(self.master, data.encode("utf-8"))

    def lflag(self):
        return termios.tcgetattr(self.slave)[3]

    def wait_exit(self):
        # The output has to be drained while waiting: a full pty buffer blocks the application's writes.
        deadline = time.monotonic() + TIMEOUT
        while self.process.poll() is None:
            if time.monotonic() > deadline:
                self.test.fail("process did not exit; output so far: %r" % self.output[-600:])
            self._read_available(0.1)
        while self._read_available(0.3):
            pass
        return self.process.returncode

    def assert_terminal_restored(self, ends_with_show_cursor=False):
        self.test.assertEqual(self.lflag() & COOKED, self.initial_lflag & COOKED, "ECHO/ICANON not restored")
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
        time.sleep(0.5)

        started = time.monotonic()
        session.send("\x03")
        self.assertEqual(session.wait_exit(), 130)
        self.assertLess(time.monotonic() - started, 5.0)
        session.assert_terminal_restored()


if __name__ == "__main__":
    unittest.main(argv=[sys.argv[0]])
