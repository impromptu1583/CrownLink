import asyncio, json, uuid, time, logging, sys

logger = logging.getLogger(__name__)


class CMClient:
    def __init__(self, addr):
        self.addr = addr # ('1.2.3.4',12345)
        self.ID = str(time.time_ns())[-16:]
        self.advertising = False
        self.last_heartbeat = time.time()
        
class ServerProtocol:
    prune_timeout = 5*60 # 5 minutes with no message
    prune_frequency = 60 # check every minute upon diagram_received

    def __init__(self,on_con_lost):
        self.on_con_lost = on_con_lost

    def connection_made(self, transport):
        # udp port successfully opened & ready for communication
        self.transport = transport
        self.CONNECTIONS = {} # { addr : CMClinet }
        self.LAST_PRUNE = time.time()

    def connection_lost(self,exc):
        logger.info("connection lost")
        self.on_con_lost = True
    
    def error_received(self,exc):
        logger.error(f"error: {exc}")

    def prune_connections(self):
        now = time.time()
        for k,v in self.CONNECTIONS.items():
            if now - v.last_heartbeat > self.prune_timeout:
                del self.CONNECTIONS[k]

    def get_client_by_ID(self,ID):
        # returns first dict item that has ID
        candidates = list({i for i in self.CONNECTIONS if self.CONNECTIONS[i].ID == ID})
        if list:
            return list[0]
        return False
    
    def datagram_received(self, data, addr):
        # if time.time() - self.LAST_PRUNE > self.prune_frequency:
        #     self.prune_connections()
        #     self.LAST_PRUNE = time.time()
        

        logger.debug(f"received: {data} from {addr}")
        
        try:
            message = data.decode()
            command,val = message.split("|",1)
        except Exception as e:
            logger.error(f"Error: {e}")
            return
        
        match command:
            case "ini":
                if not addr in self.CONNECTIONS.keys():
                    self.CONNECTIONS[addr] = CMClient(addr)
                    self.transport.sendto(self.CONNECTIONS[addr].ID.encode(), addr)
                    logger.info(f"New client connected from {addr}, given {self.CONNECTIONS[addr].ID}")
                else:
                    # TODO: need to re-init perhaps?
                    pass
            case "adv":
                try:
                    self.CONNECTIONS[addr].advertising = bool(int(val)) # send 1 for true, 0 for false
                    logger.info(f"{addr} adv set to {self.CONNECTIONS[addr].advertising}")
                except ValueError:
                    logger.error(f"Error setting advertising state, message {message}")
            case "sol":
                # send advertisers this user's solicitation
                advertisers = list(filter(lambda c:c.advertising,self.CONNECTIONS.values()))
                msg = b'sol' + self.CONNECTIONS[addr].ID.encode()
                for conn in advertisers:
                    self.transport.sendto(msg, conn.addr)
                    
            case "con":
                # con|dest uuid|ice candidates
                # replace dest uuid with src uuid then forward
                try:
                    dest_id,ice = message.split("|",1)
                    dest_conn = self.get_client_by_ID(dest_id)
                    forward_message = self.CONNECTIONS[addr].ID + "|" + ice
                    self.transport.sendto(forward_message, dest_conn.addr)
                except Exception as e:
                    logger.error(f"error con: {e}")

        print(f"received: {data}")
        try:
            print(f"Decoded: {data.decode()}")
        except:
            pass
        try:
            print(f"Int: {int.from_bytes(data,byteorder='little')}")
        except:
            pass        

async def main():
    logging.basicConfig(filename='signalingserver.log', level=logging.DEBUG)
    logger.info("Starting UDP Server")
    print("Starting UDP server")

    loop = asyncio.get_running_loop()
    on_con_lost = loop.create_future()
    transport, protocol = await loop.create_datagram_endpoint(
        lambda: ServerProtocol(on_con_lost),
        local_addr=('127.0.0.1', 9999))

    try:
        await on_con_lost
    finally:
        transport.close()


if __name__ == "__main__":
    asyncio.run(main())