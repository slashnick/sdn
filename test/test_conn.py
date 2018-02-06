import os
import os.path
import pytest
import signal
import socket
import struct
import subprocess
import time


TARGET = os.path.join(os.path.dirname(__file__), '..', 'sdn')
PORT = 8080


class Socket(socket.socket):

    def recvall(self, buffersize, flags=socket.MSG_WAITALL):
        return self.recv(buffersize, flags)


@pytest.fixture
def proc():
    p = subprocess.Popen([TARGET, str(PORT)], stdout=subprocess.PIPE,
                         stderr=subprocess.PIPE)
    yield p
    time.sleep(.1)
    p.send_signal(signal.SIGINT)
    stdout, stderr = p.communicate()
    time.sleep(.1)
    assert stdout == b''
    assert stderr == b''
    print(stdout.decode('utf-8'), end='')


def connect():
    sock = Socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.connect(('localhost', PORT))
    return sock


def test_echo(proc):
    with connect() as sock:
        packet = b'\x04\x02\x00\x14\x12\x34\x56\x78Hello world!'
        sock.sendall(packet)
        assert sock.recvall(20) == b'\x04\3\x00\x14\x12\x34\x56\x78Hello world!'


def test_big_echo(proc):
    with connect() as sock1, connect() as sock2:
        header1 = b'\x04\x02\xff\xff\xde\xad\xbe\xef'
        header2 = b'\x04\x02\xff\xff\x11\xc0\xff\xee'
        payload1 = os.urandom(65535 - len(header1))
        payload2 = os.urandom(65535 - len(header2))
        sock1.sendall(header1)
        sock2.sendall(header2)
        time.sleep(.05)
        sock1.sendall(payload1)
        assert sock1.recvall(8) == b'\x04\3\xff\xff\xde\xad\xbe\xef'
        sock2.sendall(payload2)
        assert sock1.recvall(len(payload1)) == payload1
        assert sock2.recvall(8) == b'\x04\3\xff\xff\x11\xc0\xff\xee'
        assert sock2.recvall(len(payload2)) == payload2


def test_many_echo(proc):
    with connect() as sock:
        payload = os.urandom(65535 - 8)
        for xid in range(10000):
            header = b'\x04\x02\xff\xff' + struct.pack('!I', xid)
            sock.sendall(header + payload)
        for xid in range(10000):
            header = b'\x04\x03\xff\xff' + struct.pack('!I', xid)
            assert sock.recvall(8) == header
            assert sock.recvall(len(payload)) == payload
