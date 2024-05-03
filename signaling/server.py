import asyncio, json, uuid, time
from enum import Enum






class CMClient:
    def __init__(self, addr):
        self.addr = addr # ('1.2.3.4',12345)
        self.ID = uuid.uuid4().bytes #bytes_le if little-endien... prob just sort that out client side
        self.advertising = False
        self.last_heartbeat = time.time()
        

class ServerProtocol:
    prune_timeout = 5*60 # 5 minutes with no message
    prune_frequency = 60 # check every minute upon diagram_received

    def connection_made(self, transport):
        # udp port successfully opened & ready for communication
        self.transport = transport
        self.CONNECTIONS = {} # { addr : CMClinet }
        self.LAST_PRUNE = time.time()

    def prune_connections(self):
        now = time.time()
        for k,v in self.CONNECTIONS.items():
            if now - v.last_heartbeat > self.prune_timeout:
                del self.CONNECTIONS[k]
        

    def datagram_received(self, data, addr):
        # if time.time() - self.LAST_PRUNE > self.prune_frequency:
        #     self.prune_connections()
        #     self.LAST_PRUNE = time.time()
        
        # add connection if we need to
        if not addr in self.CONNECTIONS.keys():
            self.CONNECTIONS[addr] = CMClient(addr)

        # process incoming data
        
        # incoming message(BYTES)
        #message = data.decode()

        print(f"received: {data}")
        try:
            print(f"Decoded: {data.decode()}")
        except:
            pass
        try:
            print(f"Int: {int.from_bytes(data,byteorder='little')}")
        except:
            pass

        # self.CONNECTIONS[addr].last_heartbeat = time.time()
        # command,val = message.split("|",1)
        # print(command,val)
        # match command:
        #     case "init":
        #         # new connection TODO: validate
        #         #self.CONNECTIONS[addr].ice_description = val
        #         # send back ID
        #         self.transport.sendto(self.CONNECTIONS[addr].ID, addr)
        #     case "advertising":
        #         # set advertising value True/False
        #         try:
        #             self.CONNECTIONS[addr].advertising = bool(int(val)) # send 1 for true, 0 for false
        #         except ValueError:
        #             print(f"Error setting advertising state, message {message}")
        #     case "solicit":
        #         advertisers = list(filter(lambda c:c.advertising,self.CONNECTIONS.values()))
        #         # todo need to serialize / unserialize data
        #         advlist = b''
        #         self.transport.sendto(advlist, addr)
        #         for conn in advertisers:
        #             sendbuff = b''
        #             self.transport.sendto(sendbuff, conn.addr)
        #             # need to send list of (uuid:ice_desc)
        #             # also need to send this connection info to them

        

async def main():
    print("Starting UDP server")

    loop = asyncio.get_running_loop()
    transport, protocol = await loop.create_datagram_endpoint(
        lambda: ServerProtocol(),
        local_addr=('127.0.0.1', 9999))

    try:
        await asyncio.sleep(3600)  # Serve for 1 hour.
    finally:
        transport.close()


if __name__ == "__main__":
    asyncio.run(main())