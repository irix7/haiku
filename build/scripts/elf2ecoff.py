#!/usr/bin/env python3
#
# Copyright 2026, Haiku, Inc. All rights reserved.
# Distributed under the terms of the MIT License.
#
# Converts an ELF32 big-endian MIPS executable to the ECOFF format that the
# SGI ARCS PROM loads over the network.
#
# Usage: elf2ecoff.py IN OUT [--append FILE] [--max-size BYTES]
#
# All PT_LOAD segments form one image that starts at the lowest load address.
# The executable segments are ".text", the rest is ".data", and the memory
# after the file-backed bytes is ".bss".
#
# With --append the bss is written as zero bytes, then a 4096 byte header
# ("HAIKUARC" and the payload length as a big-endian 32-bit value), then the
# payload, padded to 4096 bytes. The loader finds the header at the first
# 4096 byte boundary after its _end symbol.

import struct
import sys

FILE_HEADER_SIZE = 20
AOUT_HEADER_SIZE = 56
SECTION_HEADER_SIZE = 40
SECTION_COUNT = 3
# N_TXTOFF(OMAGIC): the headers, rounded up to 16 bytes.
TEXT_FILE_OFFSET = (FILE_HEADER_SIZE + AOUT_HEADER_SIZE
	+ SECTION_COUNT * SECTION_HEADER_SIZE + 15) & ~15

MIPSEB_MAGIC = 0x0160
OMAGIC = 0x0107
# F_RELFLG | F_EXEC | F_LNNO | F_LSYMS | F_MIPS_NO_SHARED
FILE_FLAGS = 0x100f
STYP_TEXT = 0x20
STYP_DATA = 0x40
STYP_BSS = 0x80

PT_LOAD = 1
PF_X = 1
EM_MIPS = 8

PAYLOAD_MAGIC = b"HAIKUARC"
PAYLOAD_ALIGNMENT = 4096


class Error(Exception):
	pass


class Image:
	def __init__(self, base, entry, textSize, data, bssSize):
		self.base = base
		self.entry = entry
		self.textSize = textSize
		self.data = data
		self.bssSize = bssSize


def parse_elf(elf):
	if len(elf) < 52 or elf[:4] != b"\x7fELF":
		raise Error("input is not an ELF file")
	if elf[4] != 1:
		raise Error("input is not ELF32")
	if elf[5] != 2:
		raise Error("input is not big-endian")
	machine, = struct.unpack_from(">H", elf, 18)
	if machine != EM_MIPS:
		raise Error("input is not a MIPS executable")
	entry, programHeaders = struct.unpack_from(">II", elf, 24)
	programHeaderSize, programHeaderCount = struct.unpack_from(">HH", elf, 42)

	segments = []
	for index in range(programHeaderCount):
		offset = programHeaders + index * programHeaderSize
		if offset + 32 > len(elf):
			raise Error("file is truncated at offset %#x" % offset)
		type, fileOffset, address, _, fileSize, memorySize, flags \
			= struct.unpack_from(">IIIIIII", elf, offset)
		if type != PT_LOAD or memorySize == 0:
			continue
		segments.append((address, fileOffset, fileSize, memorySize,
			(flags & PF_X) != 0))
	if not segments:
		raise Error("ELF has no loadable segment")
	segments.sort(key=lambda segment: segment[0])

	base = segments[0][0]
	fileEnd = base
	memoryEnd = base
	textEnd = base
	for address, fileOffset, fileSize, memorySize, executable in segments:
		if fileSize != 0:
			fileEnd = max(fileEnd, address + fileSize)
		memoryEnd = max(memoryEnd, address + memorySize)
		if executable and fileSize != 0:
			textEnd = max(textEnd, address + fileSize)
	if fileEnd == base:
		raise Error("ELF has no file-backed bytes")
	if memoryEnd > 0x100000000:
		raise Error("image runs past the 32-bit address space")

	data = bytearray(fileEnd - base)
	for address, fileOffset, fileSize, memorySize, executable in segments:
		if fileOffset + fileSize > len(elf):
			raise Error("segment at %#010x runs past the end of the file"
				% address)
		data[address - base:address - base + fileSize] \
			= elf[fileOffset:fileOffset + fileSize]

	if entry < base or entry >= fileEnd:
		raise Error("entry %#010x is outside the loaded image" % entry)

	# The PROM needs a non-empty .text and a non-empty .data. Split in the
	# middle when the ELF does not give a usable boundary.
	textSize = textEnd - base
	if textSize == 0 or textSize >= len(data):
		textSize = (len(data) // 2) & ~15
	if textSize == 0 or textSize >= len(data):
		raise Error("image is too small to split into .text and .data")

	return Image(base, entry, textSize, data, memoryEnd - fileEnd)


def append_payload(image, payload):
	total = len(image.data) + image.bssSize
	headerOffset = (total + PAYLOAD_ALIGNMENT - 1) & ~(PAYLOAD_ALIGNMENT - 1)
	image.data.extend(bytes(headerOffset - len(image.data)))
	header = bytearray(PAYLOAD_ALIGNMENT)
	header[:8] = PAYLOAD_MAGIC
	struct.pack_into(">I", header, 8, len(payload))
	image.data.extend(header)
	image.data.extend(payload)
	padded = (len(image.data) + PAYLOAD_ALIGNMENT - 1) \
		& ~(PAYLOAD_ALIGNMENT - 1)
	image.data.extend(bytes(padded - len(image.data)))
	image.bssSize = 0
	if image.base + len(image.data) > 0x100000000:
		raise Error("image with payload runs past the 32-bit address space")


def section_header(name, address, size, fileOffset, flags):
	# s_name, s_paddr, s_vaddr, s_size, s_scnptr, s_relptr, s_lnnoptr,
	# s_nreloc, s_nlnno, s_flags
	return struct.pack(">8sIIIIIIHHI", name, address, address, size,
		fileOffset, 0, 0, 0, 0, flags)


def write_ecoff(image):
	textSize = image.textSize
	dataSize = len(image.data) - textSize
	dataStart = (image.base + textSize) & 0xffffffff
	bssStart = (dataStart + dataSize) & 0xffffffff

	# struct filehdr: f_magic, f_nscns, f_timdat, f_symptr, f_nsyms,
	# f_opthdr, f_flags
	output = bytearray(struct.pack(">HHIIIHH", MIPSEB_MAGIC, SECTION_COUNT,
		0, 0, 0, AOUT_HEADER_SIZE, FILE_FLAGS))
	# struct aouthdr: magic, vstamp, tsize, dsize, bsize, entry, text_start,
	# data_start, bss_start, gprmask, cprmask[4], gp_value
	output += struct.pack(">HHIIIIIIIIIIIII", OMAGIC, 0, textSize, dataSize,
		image.bssSize, image.entry, image.base, dataStart, bssStart,
		0, 0, 0, 0, 0, 0)
	output += section_header(b".text", image.base, textSize,
		TEXT_FILE_OFFSET, STYP_TEXT)
	output += section_header(b".data", dataStart, dataSize,
		TEXT_FILE_OFFSET + textSize, STYP_DATA)
	output += section_header(b".bss", bssStart, image.bssSize, 0, STYP_BSS)
	output.extend(bytes(TEXT_FILE_OFFSET - len(output)))
	output += image.data
	return output


def main(arguments):
	positional = []
	payloadPath = None
	maxSize = None
	index = 0
	while index < len(arguments):
		argument = arguments[index]
		if argument in ("--append", "--max-size"):
			if index + 1 >= len(arguments):
				raise Error("%s needs a value" % argument)
			if argument == "--append":
				payloadPath = arguments[index + 1]
			else:
				maxSize = int(arguments[index + 1], 0)
			index += 2
			continue
		positional.append(argument)
		index += 1
	if len(positional) != 2:
		raise Error("usage: elf2ecoff.py IN OUT [--append FILE] "
			"[--max-size BYTES]")
	inputPath, outputPath = positional

	with open(inputPath, "rb") as file:
		image = parse_elf(file.read())
	if payloadPath is not None:
		with open(payloadPath, "rb") as file:
			append_payload(image, file.read())

	output = write_ecoff(image)
	if maxSize is not None and len(output) > maxSize:
		raise Error("%s: %d bytes is above the limit of %d bytes"
			% (outputPath, len(output), maxSize))
	with open(outputPath, "wb") as file:
		file.write(output)

	print("%s: %d bytes; text %#010x+%#x, data %#010x+%#x, bss %#x, "
		"entry %#010x" % (outputPath, len(output), image.base,
			image.textSize, image.base + image.textSize,
			len(image.data) - image.textSize, image.bssSize, image.entry))


if __name__ == "__main__":
	try:
		main(sys.argv[1:])
	except (Error, OSError) as error:
		sys.stderr.write("elf2ecoff: %s\n" % error)
		sys.exit(1)
