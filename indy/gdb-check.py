#!/usr/bin/env python3
"""Checks the kernel's GDB stub through GDB's remote serial protocol.

Usage: gdb-check.py TRANSPORT BREAK_ADDRESS

TRANSPORT is "unix:PATH" (the serial console of iris, serial_socket_b) or
"udp:HOST:PORT" (the debug console on the network, UDP port 2159 of the
kernel). BREAK_ADDRESS is the address of a kernel function that runs again
and again, in hex.

The check attaches, reads registers and memory, sets a breakpoint, resumes,
waits for the breakpoint, single-steps, lists the threads and detaches. It
prints "gdb-check: PASS" and exits with 0 if every step gave the expected
answer.
"""

import socket
import sys
import time

REGISTER_DIGITS = 16
REGISTER_COUNT = 38
REGISTER_PC = 37
TIMEOUT = 20


class Failure(Exception):
	pass


class Channel:
	"""A byte stream to the stub."""

	def __init__(self, transport):
		kind, _, rest = transport.partition(":")
		self.datagrams = kind == "udp"
		if kind == "unix":
			self.socket = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
			self.socket.connect(rest)
		elif kind == "udp":
			host, _, port = rest.rpartition(":")
			self.socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
			self.socket.connect((host, int(port)))
		else:
			raise Failure("unknown transport " + transport)
		self.input = b""

	def send(self, data):
		self.socket.sendall(data)

	def read_byte(self, timeout=TIMEOUT):
		if not self.input:
			self.socket.settimeout(timeout)
			try:
				self.input = self.socket.recv(4096)
			except socket.timeout:
				raise Failure("no answer within %d s" % timeout)
			if not self.input and not self.datagrams:
				raise Failure("connection closed")
		byte = self.input[:1]
		self.input = self.input[1:]
		return byte

	def drain(self, seconds):
		"""Reads and returns everything that arrives within the time."""
		data = self.input
		self.input = b""
		end = time.monotonic() + seconds
		while True:
			left = end - time.monotonic()
			if left <= 0:
				return data
			self.socket.settimeout(left)
			try:
				data += self.socket.recv(4096)
			except socket.timeout:
				return data


def frame(payload):
	return b"$" + payload + b"#%02x" % (sum(payload) & 0xff)


class Stub:
	def __init__(self, channel):
		self.channel = channel
		self.console = b""

	def read_packet(self, timeout=TIMEOUT):
		"""Returns the next packet that is not console output."""
		while True:
			while self.channel.read_byte(timeout) != b"$":
				pass
			payload = b""
			while True:
				byte = self.channel.read_byte(timeout)
				if byte == b"#":
					break
				payload += byte
			checksum = self.channel.read_byte() + self.channel.read_byte()
			if int(checksum, 16) != sum(payload) & 0xff:
				raise Failure("bad checksum in %r" % payload)
			if payload[:1] == b"O" and payload[1:2] != b"K":
				# console output of the running kernel, not acknowledged
				self.console += bytes.fromhex(payload[1:].decode())
				continue
			self.channel.send(b"+")
			return payload.decode()

	def send(self, text):
		self.channel.send(frame(text.encode()))
		while True:
			byte = self.channel.read_byte()
			if byte == b"+":
				return
			if byte == b"-":
				raise Failure("stub rejected %r" % text)

	def command(self, text, timeout=TIMEOUT):
		self.send(text)
		return self.read_packet(timeout)


def expect(condition, message):
	if not condition:
		raise Failure(message)
	print("gdb-check: ok    " + message.split(":")[0])


def registers_of(reply):
	expect(len(reply) == REGISTER_COUNT * REGISTER_DIGITS,
		"g gives %d registers: got %d digits" % (REGISTER_COUNT, len(reply)))
	return [int(reply[i:i + REGISTER_DIGITS], 16)
		for i in range(0, len(reply), REGISTER_DIGITS)]


def run(transport, break_address):
	channel = Channel(transport)
	stub = Stub(channel)

	if not channel.datagrams:
		# Control-C enters the kernel debugger. A '$' at its prompt starts
		# the stub.
		channel.send(b"\x03")
		text = channel.drain(3)
		expect(b"kdebug> " in text,
			"Control-C gives the KDL prompt: got %r" % text[-200:])
	# On the network the first datagram enters the kernel debugger.

	reply = stub.command("qSupported")
	expect(reply.startswith("PacketSize="), "qSupported: got %r" % reply)

	reply = stub.command("?")
	expect(reply[:1] in ("S", "T"), "? gives a stop reply: got %r" % reply)

	registers = registers_of(stub.command("g"))
	pc = registers[REGISTER_PC] & 0xffffffff
	expect(pc >= 0x80000000, "pc is a kernel address: got %#x" % pc)

	reply = stub.command("m%x,10" % break_address)
	expect(len(reply) == 32, "m reads 16 bytes: got %r" % reply)
	code = reply

	reply = stub.command("m0,4")
	expect(reply.startswith("E"), "m at address 0 gives an error: got %r"
		% reply)

	# memory writes: a scratch word below the stack pointer
	scratch = (registers[29] & 0xffffffff) - 256
	saved = stub.command("m%x,4" % scratch)
	reply = stub.command("M%x,4:cafe1234" % scratch)
	expect(reply == "OK", "M writes 4 bytes: got %r" % reply)
	reply = stub.command("m%x,4" % scratch)
	expect(reply == "cafe1234", "m reads the bytes back: got %r" % reply)
	stub.command("M%x,4:%s" % (scratch, saved))

	reply = stub.command("Z0,%x,4" % break_address)
	expect(reply == "OK", "Z0 sets a breakpoint: got %r" % reply)

	reply = stub.command("m%x,10" % break_address)
	expect(reply == code,
		"memory shows no breakpoint while stopped: got %r" % reply)

	reply = stub.command("c", 60)
	expect(reply[:3] in ("S05", "T05"),
		"c stops with SIGTRAP: got %r" % reply)

	registers = registers_of(stub.command("g"))
	pc = registers[REGISTER_PC] & 0xffffffff
	expect(pc == break_address, "pc is at the breakpoint: got %#x" % pc)

	reply = stub.command("z0,%x,4" % break_address)
	expect(reply == "OK", "z0 removes the breakpoint: got %r" % reply)

	for step in range(3):
		reply = stub.command("s", 60)
		expect(reply[:3] in ("S05", "T05"),
			"s stops with SIGTRAP: got %r" % reply)
		registers = registers_of(stub.command("g"))
		next_pc = registers[REGISTER_PC] & 0xffffffff
		expect(next_pc != pc and next_pc >= 0x80000000,
			"s moves the pc: %#x to %#x" % (pc, next_pc))
		pc = next_pc

	reply = stub.command("P4=00000000deadbeef")
	expect(reply == "OK", "P writes a register: got %r" % reply)
	reply = stub.command("p4")
	expect(reply == "00000000deadbeef", "p reads it back: got %r" % reply)
	stub.command("P4=%016x" % registers[4])

	reply = stub.command("qC")
	expect(reply.startswith("QC"), "qC names the thread: got %r" % reply)
	current = reply[2:]

	threads = []
	reply = stub.command("qfThreadInfo")
	while reply.startswith("m"):
		threads += reply[1:].split(",")
		reply = stub.command("qsThreadInfo")
	expect(reply == "l" and current in threads,
		"thread list holds the current thread: %d threads, end %r"
		% (len(threads), reply))

	reply = stub.command("qThreadExtraInfo,%s" % current)
	name = bytes.fromhex(reply).decode(errors="replace")
	expect(len(name) > 0, "thread has a description: %r" % name)

	other = [thread for thread in threads if thread != current][0]
	reply = stub.command("Hg%s" % other)
	expect(reply == "OK", "Hg selects another thread: got %r" % reply)
	registers = registers_of(stub.command("g"))
	expect(registers[29] & 0xffffffff >= 0x80000000,
		"the other thread has a kernel stack pointer: got %#x"
		% registers[29])
	stub.command("Hg%s" % current)

	# Resume, then interrupt the running kernel.
	# The kernel's console output is read meanwhile: iris drops what a slow
	# reader leaves in the serial socket.
	stub.send("c")
	channel.input = channel.drain(2)
	channel.send(b"\x03")
	reply = stub.read_packet(30)
	expect(reply[:3] in ("S02", "T02"),
		"a break stops the running kernel with SIGINT: got %r" % reply)

	reply = stub.command("D")
	expect(reply == "OK", "D detaches: got %r" % reply)

	# The kernel runs on: it can be entered again.
	time.sleep(1)
	if not channel.datagrams:
		channel.send(b"\x03")
		text = channel.drain(3)
		expect(b"kdebug> " in text,
			"the kernel runs after the detach: got %r" % text[-200:])
		channel.send(b"continue\n")
	else:
		reply = stub.command("?")
		expect(reply[:1] in ("S", "T"),
			"the kernel runs after the detach: got %r" % reply)
		reply = stub.command("D")
		expect(reply == "OK", "second detach: got %r" % reply)


def main():
	if len(sys.argv) != 3:
		sys.exit(__doc__)
	try:
		run(sys.argv[1], int(sys.argv[2], 16))
	except Failure as failure:
		print("gdb-check: FAIL  %s" % failure)
		sys.exit(1)
	print("gdb-check: PASS")


if __name__ == "__main__":
	main()
