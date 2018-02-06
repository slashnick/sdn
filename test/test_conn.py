import os.path
import pytest
import signal
import socket
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
        packet = b'\x04\x02\x00\x14\0\0\0\0Hello world!'
        sock.sendall(packet)
        assert sock.recvall(20) == b'\x04\3\x00\x14\0\0\0\0Hello world!'
