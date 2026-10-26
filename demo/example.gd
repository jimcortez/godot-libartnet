extends Node

# Example ArtNet usage
const UNIVERSE = 1  # ArtNet universe to send data on
const SEND_INTERVAL = 0.5  # Send interval in seconds

# Network configuration
# See README.md for instructions on how to find these values for your system
const BIND_ADDRESS = "192.168.2.52"  # Your computer's IP address on the network
const BROADCAST_ADDRESS = "192.168.15.255"  # Your network's broadcast address

var artnet: ArtNetController
var time_since_last_send: float = 0.0

func _ready() -> void:
	artnet = ArtNetController.new()
	
	# Configure ArtNet controller
	# Parameters: bind_address, port, net, subnet, universe, broadcast_address
	if not artnet.configure(BIND_ADDRESS, 6454, 0, 0, UNIVERSE, BROADCAST_ADDRESS):
		print("Failed to configure ArtNet controller")
		return
	
	# Start the controller (DMX sending is automatically enabled)
	if not artnet.start():
		print("Failed to start ArtNet controller")
		return
	
	print("ArtNet controller started successfully")
	print("Sending random DMX data on universe ", UNIVERSE, " every ", SEND_INTERVAL, " seconds")
	
func _process(delta: float) -> void:
	if not artnet:
		return
	
	time_since_last_send += delta
	
	if time_since_last_send >= SEND_INTERVAL:
		time_since_last_send = 0.0
		_send_dmx_data()

func _send_dmx_data() -> void:
	# Generate random DMX values for all 512 channels
	# Create DMX data (512 channels - full universe)
	var dmx_data: PackedByteArray
	dmx_data.resize(512)
	for i in range(dmx_data.size()):
		dmx_data[i] = randi() % 256
	
	# Set DMX data for the configured universe
	if artnet.set_dmx_data(UNIVERSE, dmx_data):
		# Send the DMX data
		if artnet.send_dmx():
			print("DMX data sent successfully (universe ", UNIVERSE, ")")
		else:
			print("Failed to send DMX data")
	else:
		print("Failed to set DMX data")

func _exit_tree() -> void:
	if artnet:
		artnet.stop()
		artnet = null
