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
    p.send_signal(signal.SIGINT)
    stdout, stderr = p.communicate()
    time.sleep(.1)
    assert stderr == b''
    print(stdout.decode('utf-8'), end='')


def connect():
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.connect(('localhost', PORT))
    return sock


def test_packet_handling(proc):
    lines = []
    with connect() as sock:
        packet = b'\x04\x00\x00\x00\x12\x34\x56\x78' * 2
        for byte in packet:
            sock.sendall(packet)
            time.sleep(.05)
        lines.append(proc.stdout.readline())
        lines.append(proc.stdout.readline())
        time.sleep(1)
    assert lines[0] == b'got a packet\n'

