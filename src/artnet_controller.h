#pragma once

#include "godot_cpp/classes/ref_counted.hpp"
#include "godot_cpp/classes/wrapped.hpp"
#include "godot_cpp/variant/variant.hpp"
#include "godot_cpp/variant/packed_byte_array.hpp"
#include "godot_cpp/variant/string.hpp"

#ifdef ARTNET_WASM_STUB
// WASM stub mode - forward declare artnet_node as void*
typedef void* artnet_node;
#else
#include <artnet/artnet.h>
#endif

using namespace godot;

class ArtNetController : public RefCounted {
	GDCLASS(ArtNetController, RefCounted)

protected:
	static void _bind_methods();

private:
	artnet_node node;
	bool running;
	uint8_t current_universe;
	PackedByteArray dmx_data;

public:
	ArtNetController();
	~ArtNetController() override;

	bool configure(const String &bind_address, int port, int net, int subnet, int universe, const String &broadcast_address = "255.255.255.255");
	bool start();
	void stop();
	bool is_running() const;
	bool set_dmx_data(int universe, const PackedByteArray &data);
	bool send_dmx();
};

