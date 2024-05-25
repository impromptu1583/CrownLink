import asyncio, json, uuid, time, logging, sys, base64, binascii
from enum import IntEnum
from copy import copy

logger = logging.getLogger(__name__)

# GLOBALS
SERVER_PORT = 9988
SERVER_ID = b'\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff'
CONNECTIONS = {}
DELIMINATER = b"-+"

def try_b64decode(value):
    try:
        return base64.b64decode(value)
    except Exception as e:
        logger.error(f"couldn't decode base64 from {value}, error: {e}")
        return False

class Signal_message_type(IntEnum):
    SIGNAL_START_ADVERTISING = 1
    SIGNAL_STOP_ADVERTISING = 2
    SIGNAL_REQUEST_ADVERTISERS = 3
    SIGNAL_SOLICIT_ADS = 4
    SIGNAL_GAME_AD = 5
    SIGNAL_JUICE_LOCAL_DESCRIPTION = 101
    SIGNAL_JUICE_CANDIDATE = 102
    SIGNAL_JUICE_DONE = 103
    SIGNAL_JUICE_TURN_CREDENTIALS = 104
    SERVER_SET_ID = 254
    SERVER_ECHO = 255

class Signal_packet():
    _peer_ID = b''
    message_type = 0
    data = ""
    
    def __init__(self,json_string = False):
        if json_string:
            self.as_json = json_string
            
    @property
    def peer_ID(self):
        return self._peer_ID

    @peer_ID.setter
    def peer_ID(self, value:bytes):
        if len(value) == 16:
            self._peer_ID = value;
        else:
            raise ValueError(f"bytes object should be exactly 16 bytes for ID, received {len(value)}")
        
    @property
    def peer_ID_base64(self):
        return base64.b64encode(self._peer_ID);

    @peer_ID_base64.setter
    def peer_ID_base64(self, value):
        bytesvalue = try_b64decode(value)
        if bytesvalue and len(bytesvalue) == 16:
            self._peer_ID = bytesvalue
        else:
            logger.error(f"could not set peer_id from {value}")
        
    @property
    def as_json(self):
            return json.dumps(
                {
                    "peer_ID":self.peer_ID_base64.decode(),
                    "peer_id":self.peer_ID_base64.decode(),
                    "message_type":self.message_type,
                    "data":self.data
                });
    
    @as_json.setter
    def as_json(self,json_string):
        if isinstance(json_string,bytes):
            json_string = json_string.decode()
        try:
            j = json.loads(json_string)
            if "peer_ID" in j.keys():
                self.peer_ID_base64 = j["peer_ID"]
            elif "peer_id" in j.keys():
                self.peer_ID_base64 = j["peer_id"]
            else:
                raise ValueError("neither peer_id nor peer_ID was found")
            self.message_type = j["message_type"]
            self.data = j["data"]
        except Exception as e:
            print(f"JSON ERROR: {e}, when processing string {json_string}")

class ServerProtocol(asyncio.Protocol):
    _peer_ID = b''
    remainder = None

    def __init__(self):
        self.peer_ID = uuid.uuid4().bytes

    @property
    def peer_ID(self):
        return self._peer_ID

    @peer_ID.setter
    def peer_ID(self, value:bytes):
        if len(value) == 16:
            self._peer_ID = value;
        else:
            raise ValueError("bytes object should be exactly 16 bytes for ID")
        
    @property
    def peer_ID_base64(self):
        return base64.b64encode(self._peer_ID);

    @peer_ID_base64.setter
    def peer_ID_base64(self, value):
        bytesvalue = try_b64decode(value)
        if bytesvalue and len(bytesvalue) == 16:
            self._peer_ID = bytesvalue
        else:
            logger.error(f"could not set peer_id from {value}")
            
        
    def connection_made(self, transport):
        self.transport = transport
        self.advertising = False
        self.addr = transport.get_extra_info('peername')
        global CONNECTIONS
        CONNECTIONS[self.peer_ID] = self
        logger.info(f"{self.addr} : {self.peer_ID_base64}")
        send_buffer = Signal_packet();
        send_buffer.peer_ID = SERVER_ID
        send_buffer.message_type = Signal_message_type.SERVER_SET_ID
        send_buffer.data = self.peer_ID_base64.decode()
        self.send_packet(send_buffer)

    def connection_lost(self,exc):
        logger.info(f"{self.addr} : {self.peer_ID_base64}")
        del CONNECTIONS[self.peer_ID]
    
    def error_received(self,exc):
        logger.error(f"error: {exc}")
    
    def send_packet(self, packet: Signal_packet):
        logger.debug(f"{packet.peer_ID_base64} >> {self.peer_ID_base64} ({Signal_message_type(packet.message_type).name}) : {packet.data}")
        self.transport.write(packet.as_json.encode()+DELIMINATER)
        
    def update_advertising(self, value):
        self.advertising = value
        logger.info(f"advertising set to {value}")

    def send_advertisers(self):
        advertisers = list({i for i in CONNECTIONS if CONNECTIONS[i].advertising})
        if self.peer_ID in advertisers: advertisers.remove(self.peer_ID)
        ads_b64 = []
        for peer in advertisers:
            ads_b64.append(CONNECTIONS[peer].peer_ID_base64.decode())
        logger.debug(ads_b64)
        if len(ads_b64):
            send_buffer = Signal_packet()
            send_buffer.peer_ID = SERVER_ID
            send_buffer.message_type = Signal_message_type.SIGNAL_REQUEST_ADVERTISERS
            send_buffer.data = "".join(ads_b64)
            self.send_packet(send_buffer)

    def data_received(self,data):
        
        if (self.remainder): #if a remaining partial packet exists, add it
            data = self.remainder+data
            self.remainder = None
        raw_packets = data.split(DELIMINATER)
        if raw_packets[-1][-len(DELIMINATER):] != DELIMINATER:
            self.remainder = raw_packets.pop() # remove partial packet

        for raw_packet in raw_packets:
            if not len(raw_packet):
                continue # empty packet
            packet = Signal_packet(raw_packet)
            logger.debug(f"{self.peer_ID_base64} >> {packet.peer_ID_base64} ({Signal_message_type(packet.message_type).name}) : {packet.data}")

            match(Signal_message_type(packet.message_type)):
                case Signal_message_type.SIGNAL_START_ADVERTISING:
                    self.update_advertising(True);
                    
                case Signal_message_type.SIGNAL_STOP_ADVERTISING:
                    self.update_advertising(False);
                    
                case Signal_message_type.SIGNAL_REQUEST_ADVERTISERS:
                    self.send_advertisers()
                
                case Signal_message_type.SERVER_SET_ID:
                    original_peer_id = self.peer_ID
                    self.peer_ID_base64 = packet.data
                    CONNECTIONS[self.peer_ID] = CONNECTIONS.pop(original_peer_id)
                
                case Signal_message_type.SERVER_ECHO:
                    self.send_packet(packet);

                case _:
                    # all other signals go peer for now
                    if packet.peer_ID in CONNECTIONS.keys():
                        send_buffer = copy(packet)
                        # replace from & to peer IDs
                        send_buffer.peer_ID = self.peer_ID
                        CONNECTIONS[packet.peer_ID].send_packet(send_buffer)
                    else:
                        logger.error(f"{packet.peer_ID_base64} not connected")

async def main():
    logging.basicConfig(format='[%(asctime)s][%(levelname)-5s][%(filename)s:%(lineno)s - %(funcName)18s()] %(message)s',
                        filename='signalingserver.log', level=logging.DEBUG, datefmt='%Y-%m-%d %H:%M:%S')
    logger.info("Starting TCP Server")
    print("Starting TCP server")

    loop = asyncio.get_running_loop()

    server = await loop.create_server(
        lambda: ServerProtocol(),
        None,SERVER_PORT
    )
    
    async with server:
        await server.serve_forever()



if __name__ == "__main__":
    asyncio.run(main())