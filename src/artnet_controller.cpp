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
	node = artnet_new(bind_addr_cstr.get_data(), 0);
	
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

	// Set port address (universe) for output port 0
	// Note: libartnet uses port_id 0 for the first output port
	// The universe is the combination of subnet and port address
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

