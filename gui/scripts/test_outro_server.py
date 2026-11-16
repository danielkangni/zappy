#!/usr/bin/env python3
# Minimal fake GRAPHIC-protocol server, just enough to get the GUI connected
# and then fire a "seg <team>" so the victory outro triggers without playing
# a full game to 6 level-8 players. Not a real Zappy server.
#
# Usage: test_outro_server.py [-p PORT] [-t TEAM] [-d DELAY]
import argparse
import socket
import time


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("-p", "--port", type=int, default=4343)
    ap.add_argument("-t", "--team", default="team1")
    ap.add_argument("-d", "--delay", type=float, default=1.0,
                     help="seconds to wait after the handshake before sending seg")
    args = ap.parse_args()

    srv = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    srv.bind(("127.0.0.1", args.port))
    srv.listen(1)
    print(f"[test_outro] listening on 127.0.0.1:{args.port}, waiting for the GUI...")

    conn, addr = srv.accept()
    print(f"[test_outro] GUI connected from {addr}")

    conn.sendall(b"WELCOME\n")

    # read the "GRAPHIC\n" line the client sends back
    buf = b""
    while not buf.endswith(b"\n"):
        chunk = conn.recv(1)
        if not chunk:
            print("[test_outro] client closed during handshake")
            return
        buf += chunk
    print(f"[test_outro] client said: {buf.strip().decode(errors='replace')!r}")

    conn.sendall(b"msz 10 10\n")
    conn.sendall(f"tna {args.team}\n".encode())
    conn.sendall(b"sgt 100\n")

    print(f"[test_outro] waiting {args.delay}s, then firing 'seg {args.team}' ...")
    time.sleep(args.delay)
    conn.sendall(f"seg {args.team}\n".encode())
    print("[test_outro] sent seg -> outro should be playing now (Ctrl-C to stop the server)")

    # keep the socket open so the GUI doesn't see a disconnect mid-outro
    try:
        while True:
            time.sleep(60)
    except KeyboardInterrupt:
        pass


if __name__ == "__main__":
    main()
