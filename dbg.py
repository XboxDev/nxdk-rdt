import socket
from dbg_pb2 import *
import time

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

	def mem(self, addr, size=0, data=None):
		"""Read/write system memory"""
		write = data is not None
		req = Request()
		if write:
			req.type = Request.MEM_WRITE
			req.data = data
		else:
			req.type = Request.MEM_READ
			req.size = size
		req.address = addr
		res = self._send_simple_request(req)
		return res if write else res.data

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

def main():
	xbox = Xbox()
	addr = ("127.0.0.1", 8080)
	# addr = ("10.0.1.14", 80)

	# Connect to the Xbox, display system info
	xbox.connect(addr)
	print xbox.info()

	# Print something to the screen
	xbox.debug_print("Hello!")

	# Allocate, write, read-back, free
	addr = xbox.malloc(1024)
	val = 0x5A
	print "Allocated memory at 0x%x" % addr
	xbox.mem(addr, val)
	assert(xbox.mem(addr) == val)
	xbox.free(addr)
	
	#xbox.reboot()
	xbox.disconnect()

if __name__ == '__main__':
	main()
