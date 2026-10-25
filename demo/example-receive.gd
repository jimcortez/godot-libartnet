extends Node

# Example ArtNet receive usage
const UNIVERSE = 0  # ArtNet universe to receive data on
# Note: If universe 1 doesn't work, try universe 0 (ArtNet universes are 0-indexed in some implementations)

# Network configuration
# See README.md for instructions on how to find these values for your system
const BIND_ADDRESS = "192.168.2.52"  # Your computer's IP address on the network
const BROADCAST_ADDRESS = "192.168.15.255"  # Your network's broadcast address

var artnet: ArtNetController
var last_dmx_data: PackedByteArray
var read_count: int = 0
var packet_count: int = 0

func _ready() -> void:
	artnet = ArtNetController.new()
	
	# Configure ArtNet controller
	# Parameters: bind_address, port, net, subnet, universe, broadcast_address
	if not artnet.configure(BIND_ADDRESS, 6454, 0, 0, UNIVERSE, BROADCAST_ADDRESS):
		print("Failed to configure ArtNet controller")
		return
	
	# Start the controller
	if not artnet.start():
		print("Failed to start ArtNet controller")
		return
	
	print("ArtNet receiver started successfully")
	print("Listening for DMX data on universe ", UNIVERSE)
	# Verify the configured universe address
	var configured_universe = artnet.get_universe_addr(0)
	print("Configured port 0 universe address: ", configured_universe)
	print("Expected universe in packets: ", UNIVERSE)
	print("If packets aren't matching, check:")
	print("  - Sender is sending to universe ", UNIVERSE)
	print("  - Network configuration is correct")
	print("  - Firewall allows UDP port 6454")

func _process(_delta: float) -> void:
	if not artnet:
		return
	
	# Read incoming ArtNet packets (non-blocking)
	# This processes any incoming packets and updates the DMX data buffer
	read_count += 1
	artnet.read(0)  # 0 = non-blocking, returns immediately
	
	# Get the received DMX data from port 0 (the port we configured)
	var dmx_data: PackedByteArray = artnet.get_dmx_data(0)
	
	# Debug: Print periodically to show we're running
	if read_count % 300 == 0:  # Every ~5 seconds at 60 FPS
		print("Still listening... Read cycles: ", read_count, ", DMX data size: ", dmx_data.size())
	
	# Only print if the data has changed
	if dmx_data.size() > 0 and dmx_data != last_dmx_data:
		packet_count += 1
		last_dmx_data = dmx_data
		print("=== Packet #", packet_count, " received ===")
		_print_dmx_data(dmx_data)

func _print_dmx_data(data: PackedByteArray) -> void:
	print("Received DMX data on universe ", UNIVERSE, " (", data.size(), " channels):")
	
	# Print first 20 channels (or all if less than 20)
	var channels_to_print = min(20, data.size())
	var output = "Channels: "
	for i in range(channels_to_print):
		output += str(data[i])
		if i < channels_to_print - 1:
			output += ", "
	
	if data.size() > 20:
		output += "... (and " + str(data.size() - 20) + " more)"
	
	print(output)
	
	# Print a summary of all non-zero channels
	var non_zero_channels = []
	for i in range(data.size()):
		if data[i] > 0:
			non_zero_channels.append("Ch" + str(i + 1) + ": " + str(data[i]))
	
	if non_zero_channels.size() > 0:
		print("Non-zero channels: ", ", ".join(non_zero_channels))

func _exit_tree() -> void:
	if artnet:
		artnet.stop()
		artnet = null
