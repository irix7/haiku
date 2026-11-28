#!/usr/bin/env python3
"""Runs a command in the guest through the bootstrap daemon of the image
(build/scripts/bootstrap_daemon.py, TCP port 4242 in the guest).

Usage: guest-run.py HOST_PORT COMMAND [TIMEOUT_SECONDS]

HOST_PORT is the host port that iris forwards to port 4242 of the guest. The
output of the command goes to stdout, its errors to stderr, and the exit code
is the exit code of this program. The connection is tried again for up to 300
seconds, because the daemon starts late in the boot.
"""
import os
import socket
import sys
import time


def connect(port, deadline):
    last = None
    while time.time() < deadline:
        try:
            return socket.create_connection(('127.0.0.1', port), timeout=10)
        except OSError as error:
            last = error
            time.sleep(5)
    raise SystemExit('guest-run: no connection: %s' % last)


def attempt(port, command, timeout):
    control = connect(port, time.time() + 60)
    stdio = connect(port, time.time() + 60)
    stderr = connect(port, time.time() + 60)
    control.sendall(b'%08d' % len(command) + command)
    stdio.settimeout(timeout)
    while True:
        data = stdio.recv(65536)
        if not data:
            break
        sys.stdout.buffer.write(data)
        sys.stdout.buffer.flush()
    stderr.settimeout(5)
    try:
        data = stderr.recv(65536)
        if data:
            sys.stderr.buffer.write(data)
    except OSError:
        pass
    control.settimeout(30)
    code = control.recv(16)
    return int(code or b'1')


def main():
    port = int(sys.argv[1])
    # the daemon starts with a PATH that lacks the system commands
    command = ('export PATH=/boot/system/bin:/boot/system/non-packaged/bin:$PATH; '
        + sys.argv[2]).encode()
    timeout = int(sys.argv[3]) if len(sys.argv) > 3 else 600
    # iris accepts the connection on the host before the daemon of the guest
    # runs, and resets it. Python starts slowly on the emulated CPU.
    deadline = time.time() + int(os.environ.get('GUEST_RUN_WAIT', '1500'))
    while True:
        try:
            return attempt(port, command, timeout)
        except (ConnectionError, socket.timeout) as error:
            if time.time() > deadline:
                raise SystemExit('guest-run: the daemon did not answer: %s' % error)
            time.sleep(10)


sys.exit(main())
