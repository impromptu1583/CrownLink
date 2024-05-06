import asyncio, json, uuid, time, logging, sys

logger = logging.getLogger(__name__)

# GLOBALS
SERVER_ID = b'\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff'
CONNECTIONS = {}

class ServerProtocol(asyncio.Protocol):
    def connection_made(self, transport):
        self.transport = transport
        self.ID = uuid.uuid4().bytes
        self.advertising = False
        self.addr = transport.get_extra_info('peername')
        global CONNECTIONS
        CONNECTIONS[self.ID] = self
        logger.info(f"received connection from {self.addr}")
        print(' '.join(f'{x:02x}' for x in list(self.ID)))

    def connection_lost(self,exc):
        logger.info(f"connection lost to {self.addr}")
        del CONNECTIONS[self.ID] #not sure what's going to happen here...
    
    def error_received(self,exc):
        logger.error(f"error: {exc}")
    
    def data_received(self,data):
        logger.debug(f"Data received from {self.addr}: {data}")
        data_array = list(data);
        try:
            dest_ID = bytes(data_array[:16])
            msg = data_array[16:]
        except IndexError:
            logger.error(f"{self.addr} msg {msg} shorter than 16 bytes received")
            return
        if bytes(dest_ID) == SERVER_ID:
            # message is for server
            # match first byte
            match(msg[0]):
                case 1: # START advertising
                    self.advertising = True;
                    logger.info(f"{self.addr},{str(self.ID)} started advertising")
                    self.transport.write(b'confirmed')
                case 2: # STOP advertising
                    self.advertising = False;
                    logger.info(f"{self.addr},{str(self.ID)} stopped advertising")
                case 3: # Solicitation
                    # list of dict keys (e.g. IDs)
                    advertisers = list({i for i in CONNECTIONS if CONNECTIONS[i].advertising})
                    reply = SERVER_ID + b'\x03' + b''.join(advertisers)
                    self.transport.write(reply)
                case 255: # echo, for debug
                    reply = SERVER_ID + msg
                    self.transport.write(reply)
        else:
            # message is for a peer, pass it along if we can
            if dest_ID in CONNECTIONS.keys():
                reply = self.ID + msg
                CONNECTIONS[dest_ID].transport.write(reply)

async def main():
    logging.basicConfig(filename='signalingserver.log', level=logging.DEBUG)
    logger.info("Starting TCP Server")
    print("Starting TCP server")

    loop = asyncio.get_running_loop()
    # on_con_lost = loop.create_future()
    # transport, protocol = await loop.create_datagram_endpoint(
    #     lambda: ServerProtocol(on_con_lost),
    #     local_addr=('127.0.0.1', 9999))

    server = await loop.create_server(
        lambda: ServerProtocol(),
        '127.0.0.1',9999
    )
    
    async with server:
        await server.serve_forever()

    # try:
    #     await on_con_lost
    # finally:
    #     transport.close()


if __name__ == "__main__":
    asyncio.run(main())