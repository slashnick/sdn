import os.path
import pytest
import signal
import socket
import subprocess
import time


TARGET = os.path.join(os.path.dirname(__file__), '..', 'sdn')
PORT = 8080


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
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.connect(('localhost', PORT))
    return sock


def test_packet_handling(proc):
    with connect() as sock:
        packet = b'\x04\x00\x00\x00\x12\x34\x56\x78'
        for byte in packet:
            sock.sendall(bytes([byte]))
            time.sleep(.05)
        assert proc.stdout.readline() == b'got a packet\n'
        for byte in packet:
            sock.sendall(bytes([byte]))
            time.sleep(.05)
        assert proc.stdout.readline() == b'got a packet\n'

        sock.sendall(packet * 2)
        assert proc.stdout.readline() == b'got a packet\n'
        assert proc.stdout.readline() == b'got a packet\n'
