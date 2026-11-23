/*
 * Copyright 2026, Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */


/*!	The debug console on the network: a byte stream in UDP datagrams on the
	Seeq 8003 Ethernet controller. GDB connects with
	"target remote udp:ADDRESS:2159".

	While the kernel runs, the seeq8003 driver owns the controller. It shows
	every received frame to mips_debug_net_input(). A datagram for the debug
	port is kept from the network stack, its bytes are queued, and the kernel
	enters its debugger at the end of the interrupt.

	Inside the kernel debugger this file drives the controller itself, polled,
	through its own DMA ring. It stops the receive channel on entry, and on
	exit restarts it at the descriptor of the driver where the HPC3 was. If
	that descriptor cannot be found, the channel stays stopped, and the driver
	restarts it with its next read.

	The console answers to the sender of the last datagram. Its own IP address
	is the destination of that datagram, before the first one the address the
	PROM booted with. It answers ARP requests for that address while the
	kernel debugger runs.
*/


#include <string.h>

#include <algorithm>

#include <arch/cpu.h>
#include <arch/debug.h>
#include <arch_indy.h>
#include <arch_vm_translation_map.h>
#include <boot/kernel_args.h>
#include <debug.h>
#include <kernel.h>
#include <seeq8003.h>
#include <util/KMessage.h>


static const uint16 kPort = 2159;
	// "gdbremote" in the IANA registry

static const uint32 kRingSize = 4;
static const size_t kDmaSize = ROUNDUP(
	(kRingSize + 1) * sizeof(hpc3_descriptor)
		+ (kRingSize + 1) * SEEQ_RX_BUFFER_SIZE, 4096);
static const uint32 kReceiveArmed
	= HPC3_DESC_EOP | HPC3_DESC_OWN | SEEQ_RX_MAX_COUNT;

static const size_t kEthernetHeaderSize = 14;
static const size_t kIPHeaderSize = 20;
static const size_t kUDPHeaderSize = 8;
static const size_t kHeaderSize
	= kEthernetHeaderSize + kIPHeaderSize + kUDPHeaderSize;
static const size_t kARPSize = 28;
static const size_t kMaxPayload = 1400;

// The RAM the HPC3 can reach lies at 128 MB to 384 MB.
static const uint32 kRAMBase = 0x08000000;
static const uint32 kRAMEnd = 0x18000000;

struct Peer {
	bool	known;
	uint8	station[6];
	uint32	address;
	uint16	port;
};


// The HPC3 reads and writes this memory behind the caches. It is used
// through kseg1 only.
static uint8 sDmaMemory[kDmaSize] __attribute__((aligned(4096)));

static volatile hpc3_descriptor* sRxDescriptors;
static volatile hpc3_descriptor* sTxDescriptor;
static uint8* sRxBuffers;
static uint8* sTxBuffer;
static uint32 sRxIndex;

static bool sInitialized;
static bool sDmaMemoryFlushed;
static bool sActive;
static bool sDriverWasActive;
static uint32 sDriverNextDescriptor;

static uint8 sStation[6];
static uint32 sLocalAddress;
static Peer sPeer;
static uint16 sPacketID;
static bool sBreakRequested;

static uint8 sInput[4096];
static uint32 sInputHead;
static uint32 sInputCount;
static uint8 sOutput[kMaxPayload];
static size_t sOutputLength;


static inline uint16
get16(const uint8* data)
{
	return ((uint16)data[0] << 8) | data[1];
}


static inline uint32
get32(const uint8* data)
{
	return ((uint32)get16(data) << 16) | get16(data + 2);
}


static inline void
put16(uint8* data, uint16 value)
{
	data[0] = value >> 8;
	data[1] = value;
}


static inline void
put32(uint8* data, uint32 value)
{
	put16(data, value >> 16);
	put16(data + 2, value);
}


static inline uint32
physical_of(const volatile void* kseg1Address)
{
	return (addr_t)kseg1Address - KSEG1_BASE;
}


static uint16
ip_checksum(const uint8* header, size_t length)
{
	uint32 sum = 0;
	for (size_t i = 0; i + 1 < length; i += 2)
		sum += get16(header + i);
	while ((sum >> 16) != 0)
		sum = (sum & 0xffff) + (sum >> 16);
	return ~sum;
}


//	#pragma mark - input


static void
queue_input(const uint8* data, size_t length)
{
	for (size_t i = 0; i < length && sInputCount < sizeof(sInput); i++) {
		sInput[(sInputHead + sInputCount) % sizeof(sInput)] = data[i];
		sInputCount++;
	}
}


/*!	Queues the payload of a datagram for the debug port and makes its sender
	the peer. Returns false for any other frame.
*/
static bool
handle_console_frame(const uint8* frame, size_t length)
{
	if (length < kHeaderSize || memcmp(frame, sStation, 6) != 0
		|| get16(frame + 12) != 0x0800) {
		return false;
	}

	const uint8* ip = frame + kEthernetHeaderSize;
	size_t headerLength = (ip[0] & 0x0f) * 4;
	size_t totalLength = get16(ip + 2);
	if ((ip[0] >> 4) != 4 || headerLength < kIPHeaderSize
		|| ip[9] != 17
			// UDP
		|| (get16(ip + 6) & 0x3fff) != 0
			// a fragment
		|| totalLength < headerLength + kUDPHeaderSize
		|| kEthernetHeaderSize + totalLength > length) {
		return false;
	}

	const uint8* udp = ip + headerLength;
	size_t udpLength = get16(udp + 4);
	if (get16(udp + 2) != kPort || udpLength < kUDPHeaderSize
		|| headerLength + udpLength > totalLength) {
		return false;
	}

	sPeer.known = true;
	memcpy(sPeer.station, frame + 6, 6);
	sPeer.address = get32(ip + 12);
	sPeer.port = get16(udp);
	sLocalAddress = get32(ip + 16);

	queue_input(udp + kUDPHeaderSize, udpLength - kUDPHeaderSize);
	return true;
}


/*!	Called by the seeq8003 driver for every good frame it has received, with
	interrupts off. Returns true if the frame was for the debug console: the
	driver then drops it.
*/
extern "C" bool
mips_debug_net_input(const uint8* frame, size_t length)
{
	if (!sInitialized || sActive || !handle_console_frame(frame, length))
		return false;

	sBreakRequested = true;
	return true;
}


//!	Returns whether a datagram has arrived since the last call.
bool
mips_debug_net_take_break_request()
{
	bool requested = sBreakRequested;
	sBreakRequested = false;
	return requested;
}


//	#pragma mark - the controller, polled


static void
reset_channel()
{
	// The reset clears the two configuration registers.
	uint32 dmaConfig = indy_read32(INDY_HPC3_ENET_DMACFG);
	uint32 pioConfig = indy_read32(INDY_HPC3_ENET_PIOCFG);

	indy_write32(INDY_HPC3_ENET_RX_CTRL, 0);
	indy_write32(INDY_HPC3_ENET_TX_CTRL, 0);
	indy_write32(INDY_HPC3_ENET_RESET,
		HPC3_ENET_RESET_CHANNEL | HPC3_ENET_RESET_CLRINT);
	arch_debug_snooze(1000);
	indy_write32(INDY_HPC3_ENET_RESET, 0);

	indy_write32(INDY_HPC3_ENET_DMACFG, dmaConfig);
	indy_write32(INDY_HPC3_ENET_PIOCFG, pioConfig);

	// Register bank 0 holds the station address.
	indy_write8(INDY_SEEQ_REGISTER(SEEQ_TX_COMMAND), 0);
	for (int i = 0; i < 6; i++)
		indy_write8(INDY_SEEQ_REGISTER(SEEQ_STATION_ADDRESS + i), sStation[i]);
	// The same command as the driver's: this code cannot tell a driver whose
	// channel had stopped from one that is not there.
	indy_write8(INDY_SEEQ_REGISTER(SEEQ_RX_COMMAND),
		SEEQ_RX_STATION_MULTI | SEEQ_RX_INTERRUPT_EOF);
}


static void
start_receiver()
{
	for (uint32 i = 0; i < kRingSize; i++) {
		sRxDescriptors[i].buffer
			= physical_of(sRxBuffers + i * SEEQ_RX_BUFFER_SIZE);
		sRxDescriptors[i].control = kReceiveArmed;
		sRxDescriptors[i].next
			= physical_of(&sRxDescriptors[(i + 1) % kRingSize]);
	}
	sRxIndex = 0;

	uint32 first = physical_of(&sRxDescriptors[0]);
	indy_write32(INDY_HPC3_ENET_RX_CBP, first);
	indy_write32(INDY_HPC3_ENET_RX_NDBP, first);
	indy_write32(INDY_HPC3_ENET_RX_CTRL, 0);
	indy_write32(INDY_HPC3_ENET_RX_CTRL, HPC3_ENET_CTRL_ACTIVE);
}


//!	Waits for a transmission to end. A frame takes about a millisecond.
static bool
wait_for_transmitter()
{
	for (int32 i = 0; i < 10000; i++) {
		if ((indy_read32(INDY_HPC3_ENET_TX_CTRL) & HPC3_ENET_CTRL_ACTIVE)
				== 0) {
			return true;
		}
		arch_debug_snooze(10);
	}

	indy_write32(INDY_HPC3_ENET_TX_CTRL, 0);
	return false;
}


//!	Sends the frame of \a length bytes in sTxBuffer.
static void
send_frame(size_t length)
{
	if (length < SEEQ_MIN_FRAME_SIZE) {
		memset(sTxBuffer + length, 0, SEEQ_MIN_FRAME_SIZE - length);
		length = SEEQ_MIN_FRAME_SIZE;
	}

	uint32 descriptor = physical_of(sTxDescriptor);
	sTxDescriptor->buffer = physical_of(sTxBuffer);
	sTxDescriptor->control = HPC3_DESC_EOX | HPC3_DESC_EOP | length;
	sTxDescriptor->next = descriptor;

	indy_write32(INDY_HPC3_ENET_TX_CBP, descriptor);
	indy_write32(INDY_HPC3_ENET_TX_NDBP, descriptor);
	indy_write32(INDY_HPC3_ENET_TX_CTRL, 0);
	indy_write32(INDY_HPC3_ENET_TX_CTRL, HPC3_ENET_CTRL_ACTIVE);

	wait_for_transmitter();
}


static void
send_datagram(const uint8* payload, size_t length)
{
	uint8* frame = sTxBuffer;
	memcpy(frame, sPeer.station, 6);
	memcpy(frame + 6, sStation, 6);
	put16(frame + 12, 0x0800);

	uint8* ip = frame + kEthernetHeaderSize;
	ip[0] = 0x45;
	ip[1] = 0;
	put16(ip + 2, kIPHeaderSize + kUDPHeaderSize + length);
	put16(ip + 4, sPacketID++);
	put16(ip + 6, 0x4000);
		// do not fragment
	ip[8] = 64;
	ip[9] = 17;
	put16(ip + 10, 0);
	put32(ip + 12, sLocalAddress);
	put32(ip + 16, sPeer.address);
	put16(ip + 10, ip_checksum(ip, kIPHeaderSize));

	uint8* udp = ip + kIPHeaderSize;
	put16(udp, kPort);
	put16(udp + 2, sPeer.port);
	put16(udp + 4, kUDPHeaderSize + length);
	put16(udp + 6, 0);
		// no checksum

	memcpy(udp + kUDPHeaderSize, payload, length);
	send_frame(kHeaderSize + length);
}


//!	Answers an ARP request for the address of the console.
static void
handle_arp_frame(const uint8* frame, size_t length)
{
	const uint8* arp = frame + kEthernetHeaderSize;
	if (length < kEthernetHeaderSize + kARPSize
		|| get16(frame + 12) != 0x0806
		|| get16(arp) != 1 || get16(arp + 2) != 0x0800
			// Ethernet and IPv4
		|| arp[4] != 6 || arp[5] != 4
		|| get16(arp + 6) != 1
			// a request
		|| sLocalAddress == 0 || get32(arp + 24) != sLocalAddress) {
		return;
	}

	uint8* reply = sTxBuffer;
	memcpy(reply, arp + 8, 6);
	memcpy(reply + 6, sStation, 6);
	put16(reply + 12, 0x0806);

	uint8* replyArp = reply + kEthernetHeaderSize;
	memcpy(replyArp, arp, 6);
	put16(replyArp + 6, 2);
	memcpy(replyArp + 8, sStation, 6);
	put32(replyArp + 14, sLocalAddress);
	memcpy(replyArp + 18, arp + 8, 10);
		// the sender's station and IP address

	send_frame(kEthernetHeaderSize + kARPSize);
}


//!	Takes the received frames out of the ring.
static void
poll_receiver()
{
	for (;;) {
		volatile hpc3_descriptor& descriptor = sRxDescriptors[sRxIndex];
		uint32 control = descriptor.control;

		if ((control & HPC3_DESC_OWN) != 0) {
			// A channel that has run into a descriptor it does not own
			// stops.
			if ((indy_read32(INDY_HPC3_ENET_RX_CTRL) & HPC3_ENET_CTRL_ACTIVE)
					== 0) {
				start_receiver();
			}
			return;
		}

		// The count field holds the bytes that were left over. The buffer
		// holds two pad bytes, the frame and one status byte.
		uint32 remaining = control & HPC3_DESC_COUNT_MASK;
		uint32 total = remaining < SEEQ_RX_MAX_COUNT
			? SEEQ_RX_MAX_COUNT - remaining : 0;
		const uint8* data = sRxBuffers + sRxIndex * SEEQ_RX_BUFFER_SIZE;

		if (total >= SEEQ_RX_OVERHEAD) {
			size_t length = total - SEEQ_RX_OVERHEAD;
			if (length >= kEthernetHeaderSize && length <= SEEQ_MAX_FRAME_SIZE
				&& (data[2 + length] & SEEQ_RX_STATUS_GOOD) != 0
				&& !handle_console_frame(data + 2, length)) {
				handle_arp_frame(data + 2, length);
			}
		}

		descriptor.control = kReceiveArmed;
		sRxIndex = (sRxIndex + 1) % kRingSize;
	}
}


static bool
is_descriptor_address(uint32 address)
{
	return address >= kRAMBase && address < kRAMEnd && (address & 3) == 0;
}


/*!	Restarts the receive channel on the ring of the driver. \a next is the
	next descriptor register as it was when the debugger took the channel.
*/
static void
restart_driver_ring(uint32 next)
{
	if (!is_descriptor_address(next))
		return;

	// The HPC3 was at the descriptor before "next". It continues there, if
	// that descriptor has not been filled.
	uint32 restart = next;
	uint32 address = next;
	for (int32 i = 0; i < 64; i++) {
		hpc3_descriptor* descriptor = (hpc3_descriptor*)MIPS_KSEG1(address);
		if (descriptor->next == next) {
			if ((descriptor->control & HPC3_DESC_OWN) != 0)
				restart = address;
			break;
		}

		address = descriptor->next;
		if (!is_descriptor_address(address))
			return;
	}

	indy_write32(INDY_HPC3_ENET_RX_CBP, restart);
	indy_write32(INDY_HPC3_ENET_RX_NDBP, restart);
	indy_write32(INDY_HPC3_ENET_RX_CTRL, 0);
	indy_write32(INDY_HPC3_ENET_RX_CTRL, HPC3_ENET_CTRL_ACTIVE);
}


//	#pragma mark - interface of the debug console


void
mips_debug_net_init(kernel_args* args)
{
	uint8* base = (uint8*)MIPS_KSEG1(
		mips_kseg0_to_physical(sDmaMemory));
	sRxDescriptors = (hpc3_descriptor*)base;
	sTxDescriptor = &sRxDescriptors[kRingSize];
	sRxBuffers = base + (kRingSize + 1) * sizeof(hpc3_descriptor);
	sTxBuffer = sRxBuffers + kRingSize * SEEQ_RX_BUFFER_SIZE;

	// The station address, as the seeq8003 driver determines it.
	bool valid = false;
	for (int i = 0; i < 6; i++) {
		sStation[i] = indy_read8(INDY_RTC_REGISTER(INDY_NVRAM_MAC_ADDRESS + i));
		if (sStation[i] != 0xff && sStation[i] != 0x00)
			valid = true;
	}
	if (!valid) {
		static const uint8 kFallback[6] = { 0x0a, 0x00, 0x69, 0x00, 0x00,
			0x01 };
		memcpy(sStation, kFallback, 6);
	}

	// The address the PROM booted with, if the loader passed it.
	KMessage bootVolume;
	int32 address;
	if (args->boot_volume_size > 0
		&& bootVolume.SetTo(args->boot_volume, args->boot_volume_size)
			== B_OK
		&& bootVolume.FindInt32("client IP", &address) == B_OK) {
		sLocalAddress = (uint32)address;
	}

	sInitialized = true;
}


//!	Takes the controller. Called when the kernel debugger has been entered.
void
mips_debug_net_enter()
{
	if (!sInitialized || sActive || gMipsCpuInfo.dcache_line_size == 0)
		return;

	if (!sDmaMemoryFlushed) {
		// The memory is zero, but the cache may hold some of these zeroes
		// still and would write them over what the HPC3 has stored.
		mips_dcache_writeback_invalidate((addr_t)sDmaMemory,
			sizeof(sDmaMemory));
		sDmaMemoryFlushed = true;
	}

	// A transmission of the driver ends first.
	wait_for_transmitter();

	sDriverWasActive = (indy_read32(INDY_HPC3_ENET_RX_CTRL)
		& HPC3_ENET_CTRL_ACTIVE) != 0;
	sDriverNextDescriptor = indy_read32(INDY_HPC3_ENET_RX_NDBP);
	indy_write32(INDY_HPC3_ENET_RX_CTRL, 0);

	if (!sDriverWasActive)
		reset_channel();

	start_receiver();
	sOutputLength = 0;
	sActive = true;
}


//!	Returns the controller. Called before the kernel debugger is left.
void
mips_debug_net_exit()
{
	if (!sActive)
		return;

	mips_debug_net_flush();
	wait_for_transmitter();
	indy_write32(INDY_HPC3_ENET_RX_CTRL, 0);
	sActive = false;

	if (sDriverWasActive)
		restart_driver_ring(sDriverNextDescriptor);
}


int
mips_debug_net_try_getchar()
{
	if (sInputCount == 0 && sActive) {
		mips_debug_net_flush();
		poll_receiver();
	}
	if (sInputCount == 0)
		return -1;

	uint8 c = sInput[sInputHead];
	sInputHead = (sInputHead + 1) % sizeof(sInput);
	sInputCount--;
	return c;
}


/*!	Appends to the datagram that mips_debug_net_flush() sends. Output without
	a peer to send it to is dropped.
*/
void
mips_debug_net_putchar(char c)
{
	if (!sActive || !sPeer.known)
		return;

	sOutput[sOutputLength++] = c;
	if (sOutputLength == sizeof(sOutput))
		mips_debug_net_flush();
}


void
mips_debug_net_flush()
{
	if (sOutputLength == 0)
		return;

	if (sActive && sPeer.known && sLocalAddress != 0)
		send_datagram(sOutput, sOutputLength);
	sOutputLength = 0;
}
