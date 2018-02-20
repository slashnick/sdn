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
    assert sock.recvall(8) == make_packet(0x00, b'', 0x9a020000)
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
        assert sock.recvall(8) == make_packet(5, b'', 0x09030000)


def test_switch_features(proc):
    with connect(proc) as sock:
        sock.sendall(make_packet(0, b''))
        assert sock.recvall(8) == make_packet(5, b'', 0x09030000)
        datapath_id = b'\xde\xad\xbe\xef\x1a\xc0\xff\xee'
        pad = b'\0' * 16
        sock.sendall(make_packet(6, datapath_id + pad))
        assert proc.stdout.readline() == b'New switch\n'
        assert proc.stdout.readline() == b'  Openflow version: 1.3\n'
        assert proc.stdout.readline() == b'  datapath-id: ' \
            b'0xdeadbeef1ac0ffee\n'

        multipart_req = b'\x00\x0d\0\0\0\0\0\0'
        assert sock.recvall(16) == make_packet(18, multipart_req, 0x78030000)


def test_packet_in(proc):
    with connect(proc) as sock:
        packet = b'wxyz\x03\xd8\x00\x01\x00\0\xc0\xff\xee\x22\xfa\xce'
        sock.sendall(make_packet(10, packet))
        assert proc.stdout.readline() == b'Got packet in\n'


def test_reading_reset(proc):
    with connect(proc) as sock:
        # Have the socket do a connection reset
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_LINGER,
                        struct.pack('ii', 1, 0))
        sock.send(b'\x04\3\x00\xff\0\0\0\0')

    assert proc.stderr.readline() == b'read: Connection reset by peer\n'


def test_port_status(proc):
    with connect(proc) as sock:
        port = (b'\x00\x03\x13\x37' b'\0\0\0\0' b'\xad\xbc\xe0\xc0\xff\xee'
                b'\0\0' b'GE1/0/2\0' + b'\0' * 8 + b'\0' * 4 * 8)

        sock.sendall(make_packet(12, b'\0\0\0\0\0\0\0\0' + port))
        sock.sendall(make_packet(12, b'\1\0\0\0\0\0\0\0' + port))
        sock.sendall(make_packet(12, b'\2\0\0\0\0\0\0\0' + port))

        assert proc.stdout.readline() == b'Add port GE1/0/2 (0x31337)\n'
        assert proc.stdout.readline() == b'Delete port GE1/0/2 (0x31337)\n'
        assert proc.stdout.readline() == b'Modify port GE1/0/2 (0x31337)\n'
