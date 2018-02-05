import os.path
import pytest
import signal
import socket
import subprocess


TARGET = os.path.join(os.path.dirname(__file__), '..', 'sdn')
PORT = 8080


@pytest.fixture
def proc():
    p = subprocess.Popen([TARGET, str(PORT)], stdout=subprocess.PIPE,
                         stderr=subprocess.PIPE)
    yield p
    p.send_signal(signal.SIGINT)
    stdout, stderr = p.communicate()
    print(stdout)
    assert stderr == b''


def connect():
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.connect(('localhost', PORT))
    return sock


def test_packet_handling(proc):
    with connect() as sock:
        assert proc.stdout.readline() == b'a client has connected\n'
        packet = b'\x04\x00\x00\x00\x12\x34\x56\x78'
        for byte in packet:
            sock.sendall(bytes([byte]))
        assert proc.stdout.readline() == b'got packet\n'
