#!/usr/bin/env python3
import sys
import struct
import signal
from threading import Thread
from datetime import datetime
from socketserver import ThreadingTCPServer, StreamRequestHandler

TARGET_WIDTH = 128
TARGET_HEIGHT = 64
PIXEL_PER_BYTE = 8

BIN_FRAME_SIZE = int(TARGET_WIDTH * TARGET_HEIGHT / PIXEL_PER_BYTE)

DEFAULT_PORT = 5678

REQUEST_SIZE = 4
STATUS_SUCCESS = 0
STATUS_FAILURE = 255

class EncodeHandler(StreamRequestHandler):
    bitmapList = []

    def handle(self):
        addr = self.request.getpeername()
        print('Got connection from', addr)

        while True:
            startTime = datetime.now()
            req = self.rfile.read(REQUEST_SIZE)
            if not req:
                break
            # recv request from client
            frameId = int.from_bytes(req, 'big') # bytes request

            # prepare response
            status, body = None, bytes()
            if frameId < len(EncodeHandler.bitmapList):
                status = STATUS_SUCCESS
                body = EncodeHandler.bitmapList[frameId]
            else:
                status = STATUS_FAILURE
                print('frameId out of range!')
                break
            # response header is: status(4B) + body length(4B)
            head = struct.pack('>II', status, len(body))

            # send response(header + body) to client
            self.wfile.write(head + body)

            currentTime = datetime.now()
            timeCost = currentTime - startTime
            print('frameId:', frameId, 'timeCost: %.3f' % timeCost.total_seconds())
        print('handle request from', addr, 'done!')
        self.wfile.close()

def load_video_bin(binFile):
    binFrames = []
    print('loading', binFile, '...')
    with open(binFile, 'rb') as f:
        while True:
            blob = f.read(BIN_FRAME_SIZE)
            if len(blob) == 0:
                break
            binFrames.append(blob)
    print('load', binFile, 'done!')
    return binFrames

def main():
    if len(sys.argv) < 2:
        print("Usage: {} binFile [port]\n\t".format(sys.argv[0]))
        exit(-1)

    binFile = sys.argv[1]
    port = DEFAULT_PORT if len(sys.argv) < 3 else int(sys.argv[2])

    print('binFile:', binFile, 'port:', port)
    server = ThreadingTCPServer(('', port), EncodeHandler)    

    def sigint_handler(signum, frame):
        print('sig {} raised: {}'.format(signum, frame))
        server.socket.close()
        server.shutdown()
        sys.exit(0)
    signal.signal(signal.SIGINT, sigint_handler) # main thread will handle signals

    try:
        EncodeHandler.bitmapList = load_video_bin(binFile)
        waiterThread = Thread(target=server.serve_forever, args=())
        waiterThread.start()
        waiterThread.join()
    except Exception as e:
        print('exception raised:', e)
    finally:
        server.socket.close()
        server.shutdown()

if __name__ == "__main__":
    main()
