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

def print_help():
    print "USAGE:"
    print "pftabled-client.py -s server -p port -t table_name -c [add|del|flush] [ -a entry_address -m entry_netmask ]"

def main():
    if len(sys.argv) < 9:
        print_help()
        sys.exit(1)

    addr = '0.0.0.0'
    netmask = 32
    timeout = 3600
    key = ''
    try:
        opts, args = getopt.getopt(argv,"hs:p:t:c:T:a:m:k:",["help","server=","port=","table=","command=","timeout=","address=","mask=","key="])
    except getopt.GetoptError:
        print_help()
    sys.exit(2)

    for opt, arg in opts:
        if opt == '-h':
            print_help
            sys.exit()
        elif opt in ("-s", "--server"):
            host = arg
        elif opt in ("-p", "--port"):
            port = int(sys.arg)
        elif opt in ("-t", "--table"):
            table = arg
        elif opt in ("-c", "--command"):
            command = { 'add': 1, 'del': 2, 'flush': 3 }.get(arg, 0)
        elif opt in ("-T", "--timeout"):
            timeout = int(sys.arg)
        elif opt in ("-a", "--address"):
            addr = sys.argv[5]
            m = re.search('([\d\.]+)/(\d+)', addr)
            if m:
                addr = m.group(1)
            netmask = int(m.group(2))
        elif opt in ("-k", "--key"):
            key = arg

    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    msg = struct.pack("BBxB4B4s32sI", 2, command, netmask, socket.inet_aton(addr), timeout, table, socket.htonl(int(time.time())) & 0xFFFFFFFF)
    msg = msg + hmac.new(key, msg, digestmod=sha).digest()
    s.sendto(msg, (host, port))
    s.close()
if __name__ == "__main__":
    main()
