import os
import os.path
import pytest
import signal
import socket
import struct
import subprocess
import time


TARGET = os.path.join(os.path.dirname(__file__), '..', 'sdn')


class Socket(socket.socket):

    def recvall(self, buffersize, flags=socket.MSG_WAITALL):
        return self.recv(buffersize, flags)


def make_packet(ptype, payload, xid=0x12c0ffee):
    """Make a packet."""
    header = b'\x04' + struct.pack('!BHI', ptype, 8 + len(payload), xid)
    return header + payload


@pytest.fixture
def proc():
    p = subprocess.Popen([TARGET, '0'], stdout=subprocess.PIPE,
                         stderr=subprocess.PIPE)
    port_line = p.stdout.readline()
    assert port_line.startswith(b'Listening on port ')
    p.port = int(port_line[18:-1])
    yield p
    time.sleep(.1)
    p.send_signal(signal.SIGINT)
    stdout, stderr = p.communicate()
    time.sleep(.1)
    assert stdout == b''
    assert stderr == b''
    print(stdout.decode('utf-8'), end='')


def connect(proc):
    sock = Socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.connect(('localhost', proc.port))
    return sock


def test_echo(proc):
    with connect(proc) as sock:
        payload = b'Hello world!'
        sock.sendall(make_packet(0x02, payload))
        assert sock.recvall(20) == make_packet(0x03, payload)


def test_complex_echo(proc):
    with connect(proc) as sock1, connect(proc) as sock2:
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


def test_many_big_echo(proc):
    with connect(proc) as sock:
        payload = os.urandom(0xffff - 8)
        for xid in range(100000):
            sock.sendall(make_packet(2, payload, xid=xid))
        for xid in range(100000):
            assert sock.recvall(0xffff) == make_packet(3, payload, xid=xid)


def test_hello(proc):
    with connect(proc) as sock:
        sock.sendall(make_packet(0, b''))
        assert sock.recvall(8) == make_packet(5, b'')


def test_switch_features(proc):
    with connect(proc) as sock:
        sock.sendall(make_packet(0, b''))
        assert sock.recvall(8) == make_packet(5, b'')
        datapath_id = b'\xde\xad\xbe\xef\x1a\xc0\xff\xee'
        pad = b'\0' * 16
        sock.sendall(make_packet(6, datapath_id + pad))
        assert proc.stdout.readline() == b'New switch\n'
        assert proc.stdout.readline() == b'  Openflow version: 1.3\n'
        assert proc.stdout.readline() == b'  datapath-id: ' \
            b'0xdeadbeef1ac0ffee\n'


def test_reading_reset(proc):
    """TODO"""


def test_writing_reset(proc):
    """TODO"""
