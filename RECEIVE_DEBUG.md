# ArtNet Receive Debugging Notes

## Configuration Analysis

Based on libartnet source code review:

1. **Port Configuration**: 
   - Port is enabled by `artnet_set_port_addr()` (sets `port_enabled = TRUE`)
   - Universe address is calculated as: `port->addr = ((subnet & 0x0F) << 4) | (port_addr & 0x0F)`
   - For universe=1, subnet=0: port_addr=1 → port->addr = 1 ✓

2. **Packet Comparison** (receive.c:101):
   ```c
   if (p->data.admx.universe == n->ports.out[i].port_addr &&
       n->ports.out[i].port_enabled)
   ```
   - Compares `uint16_t universe` (from packet) with `uint8_t port_addr` (configured)
   - On little-endian systems, this should work correctly

3. **Byte Order**:
   - When sending: `p.data.admx.universe = htols(port->port_addr)` - converts to little-endian
   - When receiving: Direct comparison - assumes little-endian system reads correctly
   - ArtNet uses little-endian byte order for universe field

## Potential Issues

1. **Byte Order**: The universe field might not be matching due to byte order in the packed struct
2. **Network Issues**: Packets might not be arriving at all
3. **Port Not Enabled**: Verify port is actually enabled (should be set by artnet_set_port_addr)

## Current Configuration Status

- ✅ Port 0 is configured for universe 1
- ✅ Port is enabled (confirmed by get_universe_addr returning 1)
- ✅ Network configuration appears correct
- ❓ Packets not matching - possible byte order or packet structure issue
