#!/usr/bin/env python
import socket
from dbg_pb2 import *
import time
import struct
import sys

class XboxError(Exception):
	def __init__(self, msg):
		self.msg = msg

	def __str__(self):
		return self.msg

class Xbox(object):
	def __init__(self):
		self._sock = None

	def connect(self, addr):
		"""Connect to the Xbox"""
		self._sock = socket.create_connection(addr, 5)

	def disconnect(self):
		"""Disconnect from the Xbox"""
		self._sock.close()
		self._sock = None

	def _send_simple_request(self, req):
		"""Send a simple request, expect success"""
		self._sock.send(req.SerializeToString())
		res = Response()
		res.ParseFromString(self._sock.recv(4096))
		if res.type != Response.OK:
			raise XboxError(res.msg)
		return res

	def info(self):
		"""Get system info"""
		req = Request()
		req.type = Request.SYSINFO
		return self._send_simple_request(req).info

	def reboot(self):
		"""Reboot the system"""
		msg = Request()
		msg.type = Request.REBOOT
		self._sock.send(msg.SerializeToString())

	def malloc(self, size):
		"""Allocate memory on the target"""
		req = Request()
		req.type = Request.MALLOC
		req.size = size
		return self._send_simple_request(req).address

	def free(self, addr):
		"""Free memory on the target"""
		req = Request()
		req.type = Request.FREE
		req.address = addr
		return self._send_simple_request(req)

	def mem_read(self, addr, size):
		"""read memory"""
		req = Request()
		req.type = Request.MEM_READ
		req.size = size
		req.address = addr
		return self._send_simple_request(req).data

	def mem_write(self, addr, data):
		"""write memory"""
		req = Request()
		req.type = Request.MEM_WRITE
		req.data = data
		req.address = addr
		return self._send_simple_request(req)

	def debug_print(self, string):
		"""Print a debug string to the screen"""
		req = Request()
		req.type = Request.DEBUG_PRINT
		req.msg = string
		return self._send_simple_request(req)

	def show_debug_screen(self):
		"""Show the debug screen"""
		req = Request()
		req.type = Request.SHOW_DEBUG_SCREEN
		return self._send_simple_request(req)

	def show_front_screen(self):
		"""Show the front screen"""
		req = Request()
		req.type = Request.SHOW_FRONT_SCREEN
		return self._send_simple_request(req)

	def call(self, address, stack=None):
		"""Call a function with given context"""
		req = Request()
		req.type = Request.CALL
		req.address = address
		if stack is not None:
			req.data = stack
		res = self._send_simple_request(req)
		eax = struct.unpack_from("<I", res.data, 7*4)[0]
		return eax


def main():

	if (len(sys.argv) != 2):
		print("Usage: " + sys.argv[0] + " <server>")
		sys.exit(1)

	xbox = Xbox()
	addr = (sys.argv[1], 9269)

	# Connect to the Xbox, display system info
	xbox.connect(addr)
	print(xbox.info())

	# Print something to the screen
	xbox.debug_print("Hello!")

	# Allocate, write, read-back, free
	addr = xbox.malloc(1024)
	val = 0x5A
	print("Allocated memory at 0x%x" % addr)
	xbox.mem_write(addr, bytes([val]))
	assert(xbox.mem_read(addr, 1)[0] == val)
	xbox.free(addr)

	# Inject a function which does `rdtsc; ret`.
	# RDTSC means "Read Time Stamp Counter". The Time Stamp Counter is a value,
	# which is incremented every CPU clock cycle.
	code = bytes([0x0F, 0x31, 0xC3])
	addr = xbox.malloc(len(code))
	xbox.mem_write(addr, code)

	# Repeatedly call the injected function until we have a stable timer
	last_time = None
	print("Testing call using RDTSC (please wait)")
	while True:

		# Ask the Xbox for the RDTSC value
		eax = xbox.call(addr)

		# The timer runs at 733MHz (Xbox CPU Clock speed); Convert to seconds
		current_time = eax / 733333333.33

		# This is necessary as the timer might wrap around between reads:
		# First timestamp would appear to be later than the second timestamp.
		# Also, at startup we only have one measurement, so we can't compare.
		if last_time is not None and current_time > last_time:
			break

		# We wait 1 second (this is the time we expect to measure)
		time.sleep(1.0)
		last_time = current_time

	# Print the measured time (should be ~1.0 seconds) and free function
	print("RDTSC measured %.3f seconds" % (current_time - last_time))
	xbox.free(addr)
	
	#xbox.reboot()
	xbox.disconnect()

if __name__ == '__main__':
	main()
