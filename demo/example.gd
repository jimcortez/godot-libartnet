extends Node

# ArtNet controller instance
var artnet: ArtNetController

func _ready() -> void:
	# Create ArtNet controller
	artnet = ArtNetController.new()
	
	print("ArtNet GDExtension Demo")
	print("=======================")
	
	# Configure the ArtNet controller
	# Parameters: bind_address, port, net, subnet, universe, broadcast_address
	# Using default broadcast address (255.255.255.255)
	var bind_address = "127.0.0.1"  # Change this to your network interface IP
	var port = 6454  # Standard ArtNet port
	var net = 0      # ArtNet net (0-127)
	var subnet = 0   # ArtNet subnet (0-15)
	var universe = 0 # DMX universe (0-15)
	
	print("Configuring ArtNet controller...")
	if artnet.configure(bind_address, port, net, subnet, universe):
		print("✓ ArtNet configured successfully")
		
		# Start the controller
		print("Starting ArtNet controller...")
		if artnet.start():
			print("✓ ArtNet started successfully")
			
			# Create some DMX data (512 channels)
			var dmx_data = PackedByteArray()
			dmx_data.resize(512)
			
			# Set some example channel values
			# Channel 1: Full brightness (255)
			dmx_data[0] = 255
			# Channel 2: Half brightness (128)
			dmx_data[1] = 128
			# Channel 3: Quarter brightness (64)
			dmx_data[2] = 64
			# Channel 4: Off (0)
			dmx_data[3] = 0
			
			# Set a pattern: every 10th channel at full brightness
			for i in range(0, 512, 10):
				dmx_data[i] = 255
			
			print("Setting DMX data...")
			if artnet.set_dmx_data(universe, dmx_data):
				print("✓ DMX data set successfully")
				
				# Send the DMX data
				print("Sending DMX data...")
				if artnet.send_dmx():
					print("✓ DMX data sent successfully!")
					print("Check your ArtNet receiver to see the DMX data.")
				else:
					print("✗ Failed to send DMX data")
			else:
				print("✗ Failed to set DMX data")
			
			# Clean up
			print("Stopping ArtNet controller...")
			artnet.stop()
			print("✓ ArtNet stopped")
		else:
			print("✗ Failed to start ArtNet")
	else:
		print("✗ Failed to configure ArtNet")
		print("Note: Make sure you have network permissions and the IP address is correct.")

func _exit_tree() -> void:
	# Ensure cleanup when the node is removed
	if artnet and artnet.is_running():
		artnet.stop()
