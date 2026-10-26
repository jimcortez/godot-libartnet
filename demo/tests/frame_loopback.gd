extends SceneTree
## Loopback test for the many-universe frame API.
##
## A receiver node (configure_receive + start, local packets accepted) and a
## sender (send_frame, no node) run in this one process. The sender streams
## 97,076 RGB pixels (291,228 bytes) as 572 universes of 510 channels; the
## receiver must hand back the identical bytes as one contiguous frame, then
## cope with a partial frame (published when the next sequence starts) and
## with flush_frame().
##
## Needs the extension built (scons) and a network interface (libartnet
## refuses to create a node without one). Runs headless:
##   godot --headless --path demo -s tests/frame_loopback.gd
## Prints one RESULT: {...} line and exits 0 (pass or skipped) / 1 (fail).

const UNIVERSES := 572
const CHANNELS := 510
const PIXEL_BYTES := 97076 * 3
const WAIT_MS := 2000

var _result := {"pass": false, "checks": [], "failures": []}


func _init() -> void:
	await _run()
	_result["pass"] = _result["failures"].is_empty()
	print("RESULT: " + JSON.stringify(_result))
	quit(0 if _result["pass"] else 1)


func _check(name: String, ok: bool, detail := "") -> bool:
	_result["checks"].append(name)
	if not ok:
		_result["failures"].append(name + (": " + detail if detail != "" else ""))
		printerr("FAIL " + name + " " + detail)
	return ok


func _pattern(seed: int) -> PackedByteArray:
	var data := PackedByteArray()
	data.resize(PIXEL_BYTES)
	for k in PIXEL_BYTES:
		data[k] = (k * 7 + 3 + seed * 29) & 255
	return data


## Reads until a frame is published or the deadline passes.
func _await_frame(rx: Object) -> bool:
	var deadline := Time.get_ticks_msec() + WAIT_MS
	while not rx.has_frame() and Time.get_ticks_msec() < deadline:
		rx.read(5)
		await process_frame
	return rx.has_frame()


func _run() -> void:
	if not ClassDB.class_exists("ArtNetController"):
		_check("extension loaded", false, "ArtNetController is not registered; run scons")
		return
	var rx: Object = ClassDB.instantiate("ArtNetController")
	if not rx.configure_receive(0, UNIVERSES, CHANNELS):
		# libartnet needs a non-loopback interface; offline Macs cannot run this.
		_result["skipped"] = "configure_receive failed (no network interface?)"
		print("SKIP: " + _result["skipped"])
		return
	rx.set_accept_local_packets(true)
	_check("accept local", rx.get_accept_local_packets())
	_check("receive buffer", rx.set_receive_buffer_size(4 << 20))
	if not _check("start", rx.start()):
		return
	var tx: Object = ClassDB.instantiate("ArtNetController")

	# Frame 1: a full frame arrives as one contiguous byte stream.
	var data1 := _pattern(1)
	_check("send count", tx.send_frame(0, data1) == UNIVERSES, str(tx.get_send_stats()))
	if not _check("frame 1 arrives", await _await_frame(rx), str(rx.get_receive_stats())):
		return
	var frame: PackedByteArray = rx.take_frame()
	_check("frame 1 size", frame.size() == UNIVERSES * CHANNELS, str(frame.size()))
	_check("frame 1 bytes", frame.slice(0, PIXEL_BYTES) == data1)
	var tail := frame.slice(PIXEL_BYTES)
	var tail_zero := true
	for b in tail:
		if b != 0:
			tail_zero = false
			break
	_check("frame 1 tail zero", tail_zero, str(tail.size()))
	_check("frame 1 sequence", rx.get_frame_sequence() == 1, str(rx.get_frame_sequence()))
	_check("taken", not rx.has_frame())

	# Frame 2: the next full frame replaces it.
	var data2 := _pattern(2)
	tx.send_frame(0, data2)
	_check("frame 2 arrives", await _await_frame(rx))
	frame = rx.take_frame()
	_check("frame 2 bytes", frame.slice(0, PIXEL_BYTES) == data2)
	_check("frame 2 sequence", rx.get_frame_sequence() == 2)

	# Frame 3 is cut short (11 universes); it is published when frame 4's
	# first packet shows a new sequence, and frame 4 completes normally.
	var data3 := _pattern(3)
	tx.send_frame(0, data3.slice(0, 11 * CHANNELS))
	rx.read(5)
	await process_frame
	_check("partial not published early", not rx.has_frame())
	var data4 := _pattern(4)
	tx.send_frame(0, data4)
	_check("frame 4 arrives", await _await_frame(rx))
	frame = rx.take_frame()
	_check("frame 4 bytes", frame.slice(0, PIXEL_BYTES) == data4)
	_check("frame 4 sequence", rx.get_frame_sequence() == 4)
	var stats: Dictionary = rx.get_receive_stats()
	_check("partial counted", stats.get("partial_frames", -1) == 1, str(stats))
	_check("frames counted", stats.get("frames", -1) == 4, str(stats))
	_check("packets counted", stats.get("packets", -1) == 3 * UNIVERSES + 11, str(stats))

	# Frame 5 is cut short and nothing follows: flush_frame() publishes it,
	# missing universes keeping frame 4's bytes.
	var data5 := _pattern(5)
	tx.send_frame(0, data5.slice(0, 11 * CHANNELS))
	rx.read(5)
	await process_frame
	_check("flush with pending", rx.flush_frame())
	_check("flushed frame present", rx.has_frame())
	frame = rx.take_frame()
	_check("flushed head", frame.slice(0, 11 * CHANNELS) == data5.slice(0, 11 * CHANNELS))
	_check("flushed rest stale", frame.slice(11 * CHANNELS, PIXEL_BYTES) == data4.slice(11 * CHANNELS, PIXEL_BYTES))
	_check("flushed sequence", rx.get_frame_sequence() == 5)
	_check("flush without pending", not rx.flush_frame())
	_result["send_stats"] = tx.get_send_stats()
	_result["receive_stats"] = rx.get_receive_stats()
	rx.stop()
