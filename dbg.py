#!/usr/bin/env python
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

  def call(self, address, stack, registers=None):
	  """Call a function with given context"""
	  req = Request()
	  req.type = Request.CALL
	  req.address = address
	  req.data = stack
	  #FIXME: req.registers = registers
	  res = self._send_simple_request(req)
	  out_registers = {}
	  out_registers['eax'] = res.address
	  return out_registers

def read(address, size):
	i = 0
	data = bytes()
	while True:
		remaining = size - i
		if remaining == 0:
			break
		c = min(remaining, 100) # lwip will currently choke on more data
		data += xbox.mem(address + i, size=c)
		i += c
	return data

def write(address, data):
	i = 0
	while True:
		remaining = len(data) - i
		#print(str(i) + " / " + str(len(data)))
		if remaining == 0:
			break
		c = min(remaining, 200) # lwip will currently choke on more data
		xbox.mem(address + i, data=bytes(data[i:i+c]))
		i += c

def read_u8(address):
	data = xbox.mem(address, size=1)
	return int.from_bytes(data, byteorder='little', signed=False)
def read_u16(address):
	data = xbox.mem(address, size=2)
	return int.from_bytes(data, byteorder='little', signed=False)
def read_u32(address):
	data = xbox.mem(address, size=4)
	return int.from_bytes(data, byteorder='little', signed=False)

def write_u8(address, value):
	xbox.mem(address, data=value.to_bytes(1, byteorder='little', signed=False))
def write_u16(address, value):
	xbox.mem(address, data=value.to_bytes(2, byteorder='little', signed=False))
def write_u32(address, value):
	xbox.mem(address, data=value.to_bytes(4, byteorder='little', signed=False))


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
	xbox.mem(addr, val)
	assert(xbox.mem(addr) == val)
	xbox.free(addr)
	
	#xbox.reboot()
	xbox.disconnect()

if __name__ == '__main__':
	main()
