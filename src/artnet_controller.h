#pragma once

#include "godot_cpp/classes/ref_counted.hpp"
#include "godot_cpp/classes/wrapped.hpp"
#include "godot_cpp/variant/variant.hpp"
#include "godot_cpp/variant/packed_byte_array.hpp"
#include "godot_cpp/variant/string.hpp"
#include "godot_cpp/variant/dictionary.hpp"

#include <cstdint>
#include <vector>

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

	// Frame receiving (configure_receive): every ArtDmx packet the node
	// hears is routed by its 15-bit port address into one contiguous
	// assembly buffer of universe_count x channels_per_universe bytes.
	bool accept_local;
	int rx_buffer_bytes;
	int rx_start;
	int rx_count;
	int rx_channels;
	std::vector<uint8_t> rx_assembly;
	std::vector<uint8_t> rx_seen;
	int rx_seen_count;
	int rx_current_seq;
	PackedByteArray rx_ready;
	bool rx_has_frame;
	int rx_frame_sequence;
	uint64_t rx_packets;
	uint64_t rx_frames;
	uint64_t rx_partial_frames;
	uint64_t rx_ignored;

	// Frame sending (send_frame): a plain UDP socket of our own, never
	// bound to the Art-Net port, so a sender never competes with a
	// receiver on the same host.
	int tx_socket;
	bool tx_broadcast;
	uint8_t tx_sequence;
	uint64_t tx_frames;
	uint64_t tx_packets;
	uint64_t tx_errors;

	static int _recv_handler(artnet_node n, void *pp, void *d);
	void _handle_dmx_packet(const void *packet);
	void _publish_frame();
	void _apply_loopback_filter();
	void _apply_receive_buffer();
	void _close_send_socket();

public:
	ArtNetController();
	~ArtNetController() override;

	bool configure(const String &bind_address, int port, int net, int subnet, int universe, const String &broadcast_address = "255.255.255.255");
	bool start();
	void stop();
	bool is_running() const;
	bool set_dmx_data(int universe, const PackedByteArray &data);
	bool send_dmx();

	// Receive methods (one node port)
	bool read(int timeout_ms = 0);
	PackedByteArray get_dmx_data(int port_id = 0);
	int get_universe_addr(int port_id = 0);

	// Frame receiving (many universes)
	bool configure_receive(int start_universe, int universe_count, int channels_per_universe = 510, const String &bind_address = "");
	void set_accept_local_packets(bool accept);
	bool get_accept_local_packets() const;
	bool set_receive_buffer_size(int bytes);
	bool has_frame() const;
	PackedByteArray take_frame();
	int get_frame_sequence() const;
	bool flush_frame();
	Dictionary get_receive_stats() const;

	// Frame sending (many universes)
	int send_frame(int start_universe, const PackedByteArray &data, int channels_per_universe = 510, const String &dest_address = "127.0.0.1", int dest_port = 6454);
	Dictionary get_send_stats() const;
};
