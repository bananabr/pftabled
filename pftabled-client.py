#!/usr/local/bin/python
#
# Copyright (c) 2010 Armin Wolfermann <armin@wolfermann.org>
#
# Permission to use, copy, modify, and distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
# ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
# ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
# OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
#
import hmac
import re
import sha
import socket
import struct
import sys
import time
import getopt
import hashlib

VERSION=3

def print_help():
    print "USAGE:"
    print "pftabled-client.py -s server -p port -t table_name -c [add|del|flush] [ -a entry_address[/entry_netmask] -T timeout]"

def main():
    if len(sys.argv) < 9:
        print_help()
        sys.exit(1)

    try:
        opts, args = getopt.getopt(sys.argv[1:],"hs:p:t:c:T:a:k:",["help","server=","port=","table=","command=","timeout=","address=","key="])
    except getopt.GetoptError:
        print_help()
        sys.exit(2)

    #DEFAULT VALUES
    addr = '0.0.0.0'
    netmask = 32
    timeout = 3600
    command = 1
    table = ''
    port = 56789
    host = '127.0.0.1'
    key = ''
    key_bytes = ''

    for opt, arg in opts:
        if opt == '-h':
            print_help
            sys.exit()
        elif opt in ("-s", "--server"):
            host = arg
        elif opt in ("-p", "--port"):
            port = int(arg)
        elif opt in ("-t", "--table"):
            table = arg
        elif opt in ("-c", "--command"):
            command = { 'add': 1, 'del': 2, 'flush': 3 }.get(arg, 0)
        elif opt in ("-T", "--timeout"):
            timeout = int(arg)
        elif opt in ("-a", "--address"):
            addr = arg
            m = re.search('([\d\.]+)/(\d+)', addr)
            if m:
                addr = m.group(1)
                netmask = int(m.group(2))
        elif opt in ("-k", "--key"):
            key = arg

    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    msg = struct.pack("BBxB4sI32sI", VERSION, command, netmask, socket.inet_aton(addr), timeout, table, socket.htonl(int(time.time())) & 0xFFFFFFFF)
    if(key != ''):
        f = open(key, mode='rb')
        key_bytes = f.read(20)
        f.close()
    msg = msg + hmac.new(key_bytes, msg, hashlib.sha1).digest()
    bytes_sent = 0
    bytes_sent = s.sendto(msg, (host, port))
    s.close()
    print "Sent {0} bytes to server".format(bytes_sent)

if __name__ == "__main__":
    main()
