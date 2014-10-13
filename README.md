docker-nss
==========

A libnss plugin for finding Docker containers

This is still a work in progress!

Installing
==========

    git clone git@github.com:danni/docker-nss.git
    cd docker-nss
    sudo make all install
    sudo sed -i -re 's/^(hosts: .*$)/\1 docker/' /etc/nsswitch.conf

or edit `/etc/nsswitch.conf`:

    hosts:      files dns mdns4_minimal myhostname docker
                                                   ^
Testing
=======

 * Host resolution

    LD_LIBRARY_PATH=$PWD PATH=$PWD/mocker:$PATH ./test

    LD_LIBRARY_PATH=$PWD getent hosts badger.docker
    LD_LIBRARY_PATH=$PWD getent hosts 10.0.0.0


 * Port resolution (once you have "run -p 8080 --name badger myimage")

    LD_LIBRARY_PATH=$PWD getent services badger.8080
    LD_LIBRARY_PATH=$PWD telnet localhost badger.8080
    

ToDo
====

 * Look up containers by image name/ID (not just container ID)
 * Look up container names for IP addresses
