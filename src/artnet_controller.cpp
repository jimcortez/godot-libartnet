#include "artnet_controller.h"

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/core/print_string.hpp>
#include <algorithm>
#include <cstring>

#ifdef ARTNET_WASM_STUB
// WASM stub mode - networking not supported
#else
#include <artnet/artnet.h>
#include <artnet/packets.h>
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#endif
#endif

using namespace godot;

// ArtDmx layout: ID(8) OpCode(2) ProtVer(2) Sequence(1) Physical(1)
// SubUni(1) Net(1) Length(2, big-endian) Data(2..512).
static const int ARTDMX_HEADER = 18;
static const int ARTDMX_MAX_CHANNELS = 512;
static const int ARTNET_MAX_PORT_ADDRESS = 32768; // 15-bit Net:SubUni

ArtNetController::ArtNetController() :
		node(nullptr), running(false), current_universe(0),
		accept_local(false), rx_buffer_bytes(0), rx_start(0), rx_count(0), rx_channels(0),
		rx_seen_count(0), rx_current_seq(-1), rx_has_frame(false), rx_frame_sequence(0),
		rx_packets(0), rx_frames(0), rx_partial_frames(0), rx_ignored(0),
		tx_socket(-1), tx_broadcast(false), tx_sequence(0), tx_frames(0), tx_packets(0), tx_errors(0) {
}

ArtNetController::~ArtNetController() {
	stop();
	_close_send_socket();
#ifndef ARTNET_WASM_STUB
	if (node != nullptr) {
		artnet_destroy(node);
		node = nullptr;
	}
#endif
}

void ArtNetController::_bind_methods() {
	ClassDB::bind_method(D_METHOD("configure", "bind_address", "port", "net", "subnet", "universe", "broadcast_address"), &ArtNetController::configure, DEFVAL("255.255.255.255"));
	ClassDB::bind_method(D_METHOD("start"), &ArtNetController::start);
	ClassDB::bind_method(D_METHOD("stop"), &ArtNetController::stop);
	ClassDB::bind_method(D_METHOD("is_running"), &ArtNetController::is_running);
	ClassDB::bind_method(D_METHOD("set_dmx_data", "universe", "data"), &ArtNetController::set_dmx_data);
	ClassDB::bind_method(D_METHOD("send_dmx"), &ArtNetController::send_dmx);
	ClassDB::bind_method(D_METHOD("read", "timeout_ms"), &ArtNetController::read, DEFVAL(0));
	ClassDB::bind_method(D_METHOD("get_dmx_data", "port_id"), &ArtNetController::get_dmx_data, DEFVAL(0));
	ClassDB::bind_method(D_METHOD("get_universe_addr", "port_id"), &ArtNetController::get_universe_addr, DEFVAL(0));

	// Frame receiving (many universes)
	ClassDB::bind_method(D_METHOD("configure_receive", "start_universe", "universe_count", "channels_per_universe", "bind_address"), &ArtNetController::configure_receive, DEFVAL(510), DEFVAL(""));
	ClassDB::bind_method(D_METHOD("set_accept_local_packets", "accept"), &ArtNetController::set_accept_local_packets);
	ClassDB::bind_method(D_METHOD("get_accept_local_packets"), &ArtNetController::get_accept_local_packets);
	ClassDB::bind_method(D_METHOD("set_receive_buffer_size", "bytes"), &ArtNetController::set_receive_buffer_size);
	ClassDB::bind_method(D_METHOD("has_frame"), &ArtNetController::has_frame);
	ClassDB::bind_method(D_METHOD("take_frame"), &ArtNetController::take_frame);
	ClassDB::bind_method(D_METHOD("get_frame_sequence"), &ArtNetController::get_frame_sequence);
	ClassDB::bind_method(D_METHOD("flush_frame"), &ArtNetController::flush_frame);
	ClassDB::bind_method(D_METHOD("get_receive_stats"), &ArtNetController::get_receive_stats);

	// Frame sending (many universes)
	ClassDB::bind_method(D_METHOD("send_frame", "start_universe", "data", "channels_per_universe", "dest_address", "dest_port"), &ArtNetController::send_frame, DEFVAL(510), DEFVAL("127.0.0.1"), DEFVAL(6454));
	ClassDB::bind_method(D_METHOD("get_send_stats"), &ArtNetController::get_send_stats);
}

bool ArtNetController::configure(const String &bind_address, int port, int net, int subnet, int universe, const String &broadcast_address) {
#ifdef ARTNET_WASM_STUB
	print_error("ArtNetController: ArtNet networking is not supported on WebAssembly/WASM platform.");
	return false;
#else
	if (node != nullptr) {
		stop();
		artnet_destroy(node);
		node = nullptr;
	}

	// An empty address lets libartnet pick the first usable interface.
	CharString bind_addr_cstr = bind_address.utf8();
	const char *bind_addr = bind_address.is_empty() ? nullptr : bind_addr_cstr.get_data();

	// Create new artnet node (verbose = 0 for production, 1 for debug)
	node = artnet_new(bind_addr, 0);

	if (node == nullptr) {
		print_error(vformat("ArtNetController: Failed to create artnet node - %s", artnet_strerror()));
		return false;
	}

	// Set subnet address
	if (artnet_set_subnet_addr(node, static_cast<uint8_t>(subnet)) != 0) {
		print_error(vformat("ArtNetController: Failed to set subnet address - %s", artnet_strerror()));
		artnet_destroy(node);
		node = nullptr;
		return false;
	}

	// Set port address for output port 0 to receive DMX data
	// Note: libartnet uses port_id 0 for the first output port
	// The universe is the combination of subnet and port address:
	// universe = (subnet << 4) | port_addr
	// So for a given universe value, we extract:
	//   - subnet = (universe >> 4) & 0x0F (upper 4 bits)
	//   - port_addr = universe & 0x0F (lower 4 bits)
	// However, since we're passed subnet separately, we use the lower 4 bits of universe as port_addr
	// The artnet_set_port_addr function will then recalculate the full universe address
	// as: port.addr = ((subnet & 0x0F) << 4) | (port_addr & 0x0F)
	// This ensures port.addr matches the universe value in incoming ArtNet packets
	uint8_t port_addr = static_cast<uint8_t>(universe & 0x0F); // Lower 4 bits for port address
	if (artnet_set_port_addr(node, 0, ARTNET_OUTPUT_PORT, port_addr) != 0) {
		print_error(vformat("ArtNetController: Failed to set port address - %s", artnet_strerror()));
		artnet_destroy(node);
		node = nullptr;
		return false;
	}

	current_universe = static_cast<uint8_t>(universe);
	running = false;
	_apply_loopback_filter();

	return true;
#endif
}

bool ArtNetController::start() {
#ifdef ARTNET_WASM_STUB
	print_error("ArtNetController: ArtNet networking is not supported on WebAssembly/WASM platform.");
	return false;
#else
	if (node == nullptr) {
		print_error("ArtNetController: Cannot start - node not configured. Call configure() or configure_receive() first.");
		return false;
	}

	if (running) {
		UtilityFunctions::push_warning("ArtNetController: Already running.");
		return true;
	}

	int result = artnet_start(node);
	if (result != 0) {
		print_error(vformat("ArtNetController: Failed to start - %s", artnet_strerror()));
		return false;
	}

	running = true;
	_apply_receive_buffer();
	return true;
#endif
}

void ArtNetController::stop() {
#ifdef ARTNET_WASM_STUB
	return;
#else
	if (node == nullptr || !running) {
		return;
	}

	artnet_stop(node);
	running = false;
#endif
}

bool ArtNetController::is_running() const {
	return running && node != nullptr;
}

bool ArtNetController::set_dmx_data(int universe, const PackedByteArray &data) {
	if (data.size() > 512) {
		print_error(vformat("ArtNetController: DMX data exceeds 512 channels (got %d)", data.size()));
		return false;
	}

	if (data.size() == 0) {
		UtilityFunctions::push_warning("ArtNetController: DMX data is empty");
		return false;
	}

	// Store the data and universe
	dmx_data = data;
	current_universe = static_cast<uint8_t>(universe);

	return true;
}

bool ArtNetController::send_dmx() {
#ifdef ARTNET_WASM_STUB
	print_error("ArtNetController: ArtNet networking is not supported on WebAssembly/WASM platform.");
	return false;
#else
	if (node == nullptr) {
		print_error("ArtNetController: Cannot send DMX - node not configured.");
		return false;
	}

	if (!running) {
		print_error("ArtNetController: Cannot send DMX - node not started. Call start() first.");
		return false;
	}

	if (dmx_data.size() == 0) {
		UtilityFunctions::push_warning("ArtNetController: No DMX data to send. Call set_dmx_data() first.");
		return false;
	}

	// Get the raw data pointer
	const uint8_t *data_ptr = dmx_data.ptr();
	int16_t length = static_cast<int16_t>(dmx_data.size());

	// Send DMX data on port 0 (first output port)
	int result = artnet_send_dmx(node, 0, length, data_ptr);

	if (result != 0) {
		print_error(vformat("ArtNetController: Failed to send DMX - %s", artnet_strerror()));
		return false;
	}

	return true;
#endif
}

bool ArtNetController::read(int timeout_ms) {
#ifdef ARTNET_WASM_STUB
	print_error("ArtNetController: ArtNet networking is not supported on WebAssembly/WASM platform.");
	return false;
#else
	if (node == nullptr) {
		print_error("ArtNetController: Cannot read - node not configured.");
		return false;
	}

	if (!running) {
		print_error("ArtNetController: Cannot read - node not started. Call start() first.");
		return false;
	}

	// libartnet's own timeout is in whole seconds, so wait here with
	// millisecond resolution and then drain whatever has arrived.
	if (timeout_ms > 0) {
		artnet_socket_t sd = artnet_get_sd(node);
		fd_set rset;
		FD_ZERO(&rset);
		FD_SET(sd, &rset);
		struct timeval tv;
		tv.tv_sec = timeout_ms / 1000;
		tv.tv_usec = (timeout_ms % 1000) * 1000;
		select(static_cast<int>(sd) + 1, &rset, nullptr, nullptr, &tv);
	}

	// Reads every pending packet and runs the handlers for each.
	int result = artnet_read(node, 0);

	// ARTNET_EOK (0) means success, negative values are errors
	if (result < 0) {
		print_error(vformat("ArtNetController: Failed to read - %s", artnet_strerror()));
		return false;
	}

	return true;
#endif
}

PackedByteArray ArtNetController::get_dmx_data(int port_id) {
	PackedByteArray result;

#ifdef ARTNET_WASM_STUB
	print_error("ArtNetController: ArtNet networking is not supported on WebAssembly/WASM platform.");
	return result;
#else
	if (node == nullptr) {
		print_error("ArtNetController: Cannot get DMX data - node not configured.");
		return result;
	}

	if (!running) {
		print_error("ArtNetController: Cannot get DMX data - node not started. Call start() first.");
		return result;
	}

	// ARTNET_MAX_PORTS is 4
	if (port_id < 0 || port_id >= 4) {
		print_error(vformat("ArtNetController: Port ID out of bounds (got %d, must be 0-3)", port_id));
		return result;
	}

	int length = 0;
	uint8_t *data = artnet_read_dmx(node, port_id, &length);

	if (data != nullptr && length > 0) {
		result.resize(length);
		memcpy(result.ptrw(), data, length);
	}

	return result;
#endif
}

int ArtNetController::get_universe_addr(int port_id) {
#ifdef ARTNET_WASM_STUB
	print_error("ArtNetController: ArtNet networking is not supported on WebAssembly/WASM platform.");
	return -1;
#else
	if (node == nullptr) {
		print_error("ArtNetController: Cannot get universe address - node not configured.");
		return -1;
	}

	// ARTNET_MAX_PORTS is 4
	if (port_id < 0 || port_id >= 4) {
		print_error(vformat("ArtNetController: Port ID out of bounds (got %d, must be 0-3)", port_id));
		return -1;
	}

	return artnet_get_universe_addr(node, port_id, ARTNET_OUTPUT_PORT);
#endif
}

// --- Frame receiving --------------------------------------------------------

bool ArtNetController::configure_receive(int start_universe, int universe_count, int channels_per_universe, const String &bind_address) {
#ifdef ARTNET_WASM_STUB
	print_error("ArtNetController: ArtNet networking is not supported on WebAssembly/WASM platform.");
	return false;
#else
	if (universe_count < 1 || start_universe < 0 || start_universe + universe_count > ARTNET_MAX_PORT_ADDRESS) {
		print_error(vformat("ArtNetController: universes %d..%d are outside the 15-bit port address range", start_universe, start_universe + universe_count - 1));
		return false;
	}
	if (channels_per_universe < 1 || channels_per_universe > ARTDMX_MAX_CHANNELS) {
		print_error(vformat("ArtNetController: channels_per_universe must be 1..512 (got %d)", channels_per_universe));
		return false;
	}

	if (node != nullptr) {
		stop();
		artnet_destroy(node);
		node = nullptr;
	}

	CharString bind_addr_cstr = bind_address.utf8();
	const char *bind_addr = bind_address.is_empty() ? nullptr : bind_addr_cstr.get_data();
	node = artnet_new(bind_addr, 0);
	if (node == nullptr) {
		print_error(vformat("ArtNetController: Failed to create artnet node - %s", artnet_strerror()));
		return false;
	}

	// Every packet goes through our handler before libartnet's own port
	// logic, so any number of universes can be received.
	if (artnet_set_handler(node, ARTNET_RECV_HANDLER, &ArtNetController::_recv_handler, this) != 0) {
		print_error(vformat("ArtNetController: Failed to set receive handler - %s", artnet_strerror()));
		artnet_destroy(node);
		node = nullptr;
		return false;
	}

	rx_start = start_universe;
	rx_count = universe_count;
	rx_channels = channels_per_universe;
	rx_assembly.assign(static_cast<size_t>(rx_count) * rx_channels, 0);
	rx_seen.assign(rx_count, 0);
	rx_seen_count = 0;
	rx_current_seq = -1;
	rx_ready = PackedByteArray();
	rx_has_frame = false;
	rx_frame_sequence = 0;
	rx_packets = rx_frames = rx_partial_frames = rx_ignored = 0;
	running = false;
	_apply_loopback_filter();
	return true;
#endif
}

void ArtNetController::set_accept_local_packets(bool accept) {
	accept_local = accept;
	_apply_loopback_filter();
}

bool ArtNetController::get_accept_local_packets() const {
	return accept_local;
}

void ArtNetController::_apply_loopback_filter() {
#ifndef ARTNET_WASM_STUB
	if (node != nullptr) {
		artnet_set_loopback_filter(node, accept_local ? 0 : 1);
	}
#endif
}

bool ArtNetController::set_receive_buffer_size(int bytes) {
	if (bytes <= 0) {
		return false;
	}
	rx_buffer_bytes = bytes;
	if (running) {
		_apply_receive_buffer();
	}
	return true;
}

void ArtNetController::_apply_receive_buffer() {
#ifndef ARTNET_WASM_STUB
	if (node == nullptr || !running || rx_buffer_bytes <= 0) {
		return;
	}
	artnet_socket_t sd = artnet_get_sd(node);
	// The kernel caps the size (macOS: kern.ipc.maxsockbuf); halve until it takes.
	int want = rx_buffer_bytes;
	while (want >= 65536) {
		if (setsockopt(sd, SOL_SOCKET, SO_RCVBUF, reinterpret_cast<const char *>(&want), sizeof(want)) == 0) {
			return;
		}
		want /= 2;
	}
	UtilityFunctions::push_warning("ArtNetController: could not enlarge the receive buffer");
#endif
}

int ArtNetController::_recv_handler(artnet_node, void *pp, void *d) {
#ifdef ARTNET_WASM_STUB
	return 1;
#else
	ArtNetController *self = static_cast<ArtNetController *>(d);
	artnet_packet p = static_cast<artnet_packet>(pp);
	switch (p->type) {
		case ARTNET_DMX:
			self->_handle_dmx_packet(p);
			return 1; // handled; skip libartnet's 4-port logic
		case ARTNET_POLL:
		case ARTNET_REPLY:
			return 0; // let the node answer polls as usual
		default:
			return 1; // unknown to libartnet; swallow instead of printing
	}
#endif
}

void ArtNetController::_handle_dmx_packet(const void *packet) {
#ifndef ARTNET_WASM_STUB
	const artnet_packet p = static_cast<const artnet_packet>(const_cast<void *>(packet));
	rx_packets++;
	if (rx_count <= 0) {
		return;
	}
	// The 15-bit port address is little-endian on the wire: SubUni, Net.
	const uint8_t *addr_bytes = reinterpret_cast<const uint8_t *>(&p->data.admx.universe);
	int address = addr_bytes[0] | (addr_bytes[1] << 8);
	int u = address - rx_start;
	if (u < 0 || u >= rx_count) {
		rx_ignored++;
		return;
	}
	int length = (p->data.admx.lengthHi << 8) | p->data.admx.length;
	int available = p->length - ARTDMX_HEADER;
	length = std::min(length, std::max(available, 0));
	length = std::min(length, std::min(rx_channels, ARTDMX_MAX_CHANNELS));

	int sequence = p->data.admx.sequence;
	// A new sequence byte or a repeated universe means the previous frame is
	// complete (or as complete as it will get).
	if (rx_seen_count > 0 && (sequence != rx_current_seq || rx_seen[u])) {
		_publish_frame();
	}
	rx_current_seq = sequence;

	uint8_t *dst = rx_assembly.data() + static_cast<size_t>(u) * rx_channels;
	memcpy(dst, p->data.admx.data, length);
	if (length < rx_channels) {
		memset(dst + length, 0, rx_channels - length);
	}
	if (!rx_seen[u]) {
		rx_seen[u] = 1;
		rx_seen_count++;
	}
	if (u == rx_count - 1) {
		_publish_frame();
	}
#endif
}

void ArtNetController::_publish_frame() {
	if (rx_seen_count == 0) {
		return;
	}
	rx_ready.resize(static_cast<int64_t>(rx_assembly.size()));
	memcpy(rx_ready.ptrw(), rx_assembly.data(), rx_assembly.size());
	rx_frame_sequence = rx_current_seq;
	rx_has_frame = true;
	rx_frames++;
	if (rx_seen_count < rx_count) {
		rx_partial_frames++;
	}
	std::fill(rx_seen.begin(), rx_seen.end(), 0);
	rx_seen_count = 0;
}

bool ArtNetController::has_frame() const {
	return rx_has_frame;
}

PackedByteArray ArtNetController::take_frame() {
	if (!rx_has_frame) {
		return PackedByteArray();
	}
	rx_has_frame = false;
	return rx_ready;
}

int ArtNetController::get_frame_sequence() const {
	return rx_frame_sequence;
}

bool ArtNetController::flush_frame() {
	if (rx_seen_count == 0) {
		return false;
	}
	_publish_frame();
	return true;
}

Dictionary ArtNetController::get_receive_stats() const {
	Dictionary d;
	d["packets"] = static_cast<int64_t>(rx_packets);
	d["frames"] = static_cast<int64_t>(rx_frames);
	d["partial_frames"] = static_cast<int64_t>(rx_partial_frames);
	d["ignored"] = static_cast<int64_t>(rx_ignored);
	d["pending"] = rx_seen_count;
	d["last_sequence"] = rx_frame_sequence;
	d["universe_count"] = rx_count;
	d["channels_per_universe"] = rx_channels;
	return d;
}

// --- Frame sending ------------------------------------------------------------

void ArtNetController::_close_send_socket() {
#ifndef ARTNET_WASM_STUB
	if (tx_socket >= 0) {
#ifdef _WIN32
		closesocket(static_cast<SOCKET>(tx_socket));
#else
		close(tx_socket);
#endif
		tx_socket = -1;
		tx_broadcast = false;
	}
#endif
}

int ArtNetController::send_frame(int start_universe, const PackedByteArray &data, int channels_per_universe, const String &dest_address, int dest_port) {
#ifdef ARTNET_WASM_STUB
	print_error("ArtNetController: ArtNet networking is not supported on WebAssembly/WASM platform.");
	return -1;
#else
	if (data.size() == 0) {
		return 0;
	}
	if (channels_per_universe < 1 || channels_per_universe > ARTDMX_MAX_CHANNELS) {
		print_error(vformat("ArtNetController: channels_per_universe must be 1..512 (got %d)", channels_per_universe));
		return -1;
	}
	int universes = static_cast<int>((data.size() + channels_per_universe - 1) / channels_per_universe);
	if (start_universe < 0 || start_universe + universes > ARTNET_MAX_PORT_ADDRESS) {
		print_error(vformat("ArtNetController: universes %d..%d are outside the 15-bit port address range", start_universe, start_universe + universes - 1));
		return -1;
	}
	if (dest_port <= 0 || dest_port > 65535) {
		print_error(vformat("ArtNetController: invalid destination port %d", dest_port));
		return -1;
	}

	if (tx_socket < 0) {
		tx_socket = static_cast<int>(socket(AF_INET, SOCK_DGRAM, 0));
		if (tx_socket < 0) {
			print_error("ArtNetController: could not create the send socket");
			return -1;
		}
	}

	struct sockaddr_in dest;
	memset(&dest, 0, sizeof(dest));
	dest.sin_family = AF_INET;
	dest.sin_port = htons(static_cast<uint16_t>(dest_port));
	CharString dest_cstr = dest_address.utf8();
	if (inet_pton(AF_INET, dest_cstr.get_data(), &dest.sin_addr) != 1) {
		print_error(vformat("ArtNetController: invalid destination address '%s'", dest_address));
		return -1;
	}
	bool broadcast = (ntohl(dest.sin_addr.s_addr) & 0xFF) == 0xFF;
	if (broadcast && !tx_broadcast) {
		int one = 1;
		setsockopt(tx_socket, SOL_SOCKET, SO_BROADCAST, reinterpret_cast<const char *>(&one), sizeof(one));
		tx_broadcast = true;
	}

	// One sequence byte per frame, 1..255 (0 means "sequence unused").
	tx_sequence = static_cast<uint8_t>((tx_sequence % 255) + 1);

	uint8_t packet[ARTDMX_HEADER + ARTDMX_MAX_CHANNELS];
	memcpy(packet, "Art-Net", 8); // includes the terminating NUL
	packet[8] = 0x00; // OpDmx 0x5000, little-endian
	packet[9] = 0x50;
	packet[10] = 0; // ProtVer 14, big-endian
	packet[11] = 14;
	packet[12] = tx_sequence;
	packet[13] = 0; // Physical

	const uint8_t *src = data.ptr();
	const int64_t total = data.size();
	int sent = 0;
	for (int u = 0; u < universes; u++) {
		int address = start_universe + u;
		int64_t offset = static_cast<int64_t>(u) * channels_per_universe;
		int length = static_cast<int>(std::min<int64_t>(channels_per_universe, total - offset));
		int padded = length + (length & 1); // Art-Net wants an even length
		packet[14] = static_cast<uint8_t>(address & 0xFF);
		packet[15] = static_cast<uint8_t>((address >> 8) & 0x7F);
		packet[16] = static_cast<uint8_t>(padded >> 8);
		packet[17] = static_cast<uint8_t>(padded & 0xFF);
		memcpy(packet + ARTDMX_HEADER, src + offset, length);
		if (padded > length) {
			packet[ARTDMX_HEADER + length] = 0;
		}
		int size = ARTDMX_HEADER + padded;
		ssize_t written = sendto(tx_socket, reinterpret_cast<const char *>(packet), size, 0, reinterpret_cast<struct sockaddr *>(&dest), sizeof(dest));
		if (written == size) {
			sent++;
		} else {
			tx_errors++;
		}
	}
	tx_frames++;
	tx_packets += sent;
	return sent;
#endif
}

Dictionary ArtNetController::get_send_stats() const {
	Dictionary d;
	d["frames"] = static_cast<int64_t>(tx_frames);
	d["packets"] = static_cast<int64_t>(tx_packets);
	d["errors"] = static_cast<int64_t>(tx_errors);
	d["last_sequence"] = static_cast<int>(tx_sequence);
	return d;
}
