from mininet.net import Mininet
from mininet.node import RemoteController
from mininet.cli import CLI
import sys


def main():
    net = Mininet()

    h1 = net.addHost('h1')
    h2 = net.addHost('h2')
    h3 = net.addHost('h3')
    h4 = net.addHost('h4')
    h5 = net.addHost('h5')

    s1 = net.addSwitch('sw1', protocols='OpenFlow13')
    s2 = net.addSwitch('sw2', protocols='OpenFlow13')
    s3 = net.addSwitch('sw3', protocols='OpenFlow13')
    s4 = net.addSwitch('sw4', protocols='OpenFlow13')
    s5 = net.addSwitch('sw5', protocols='OpenFlow13')

    c0 = net.addController('c0',
                   controller=RemoteController,
                   ip='127.0.0.1',
                   port=int(sys.argv[1]))

    net.addLink(s1, s2)
    net.addLink(s1, s4)
    net.addLink(s2, s3)
    net.addLink(s2, s4)
    net.addLink(s3, s4)
    net.addLink(s4, s5)

    for sw, h in zip((s1, s2, s3, s4, s5), (h1, h2, h3, h4, h5)):
        net.addLink(sw, h)

    net.start()
    CLI(net)
    net.stop()


if __name__ == '__main__':
    main()
