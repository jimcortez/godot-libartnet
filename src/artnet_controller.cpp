#include "artnet_controller.h"

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/core/print_string.hpp>
#include <cstring>

#ifdef ARTNET_WASM_STUB
// WASM stub mode - networking not supported
#else
#include <artnet/artnet.h>
#endif

using namespace godot;

ArtNetController::ArtNetController() : node(nullptr), running(false), current_universe(0) {
}

ArtNetController::~ArtNetController() {
	stop();
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
}

bool ArtNetController::configure(const String &bind_address, int port, int net, int subnet, int universe, const String &broadcast_address) {
#ifdef ARTNET_WASM_STUB
	print_error("ArtNetController: ArtNet networking is not supported on WebAssembly/WASM platform.");
	return false;
#else
	// Stop and cleanup existing node if any
	if (node != nullptr) {
		stop();
		artnet_destroy(node);
		node = nullptr;
	}

	// Convert Godot String to C string
	CharString bind_addr_cstr = bind_address.utf8();
	
	// Create new artnet node (verbose = 0 for production, 1 for debug)
	// Use verbose=1 temporarily for debugging
	node = artnet_new(bind_addr_cstr.get_data(), 0);
	
	if (node == nullptr) {
		print_error(vformat("ArtNetController: Failed to create artnet node - %s", artnet_strerror()));
		return false;
	}

	// Set subnet address
	// Note: The subnet parameter is passed but we should verify it matches the universe
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
	// IMPORTANT: For universe 1 with subnet 0: port_addr = 1, and port.addr will be calculated as:
	// port.addr = ((0 & 0x0F) << 4) | (1 & 0x0F) = (0 << 4) | 1 = 1
	// This should match incoming packets with universe field = 1 (in little-endian uint16_t format)
	uint8_t port_addr = static_cast<uint8_t>(universe & 0x0F); // Lower 4 bits for port address
	if (artnet_set_port_addr(node, 0, ARTNET_OUTPUT_PORT, port_addr) != 0) {
		print_error(vformat("ArtNetController: Failed to set port address - %s", artnet_strerror()));
		artnet_destroy(node);
		node = nullptr;
		return false;
	}

	current_universe = static_cast<uint8_t>(universe);
	running = false;

	return true;
#endif
}

bool ArtNetController::start() {
#ifdef ARTNET_WASM_STUB
	print_error("ArtNetController: ArtNet networking is not supported on WebAssembly/WASM platform.");
	return false;
#else
	if (node == nullptr) {
		print_error("ArtNetController: Cannot start - node not configured. Call configure() first.");
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
	return true;
#endif
}

void ArtNetController::stop() {
#ifdef ARTNET_WASM_STUB
	// No-op for WASM
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

	// Convert timeout from milliseconds to seconds
	// artnet_read expects timeout in seconds (0 for non-blocking)
	int timeout = timeout_ms > 0 ? (timeout_ms + 500) / 1000 : 0; // Round up to nearest second
	int result = artnet_read(node, timeout);
	
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

