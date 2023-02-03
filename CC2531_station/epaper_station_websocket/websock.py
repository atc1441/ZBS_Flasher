import websockets
import websockets.server
import queue
import asyncio
import time

#Credit: https://stackoverflow.com/questions/68939894/implement-a-python-websocket-listener-without-async-asyncio
class SynchronousWebsocketServer:
    """
    Synchronous wrapper around asynchronous websockets server by Pier-Yves Lessard
    """
    def __init__(self, connect_callback=None, disconnect_callback=None):
        self.rxqueue = queue.Queue()
        self.txqueue = queue.Queue()
        self.loop = asyncio.new_event_loop()
        self.ws_server = None
        self.connect_callback = connect_callback
        self.disconnect_callback = disconnect_callback

    # Executed for each websocket
    async def server_routine(self, websocket, path):
        if self.connect_callback is not None:
            self.connect_callback(websocket)

        try:
            async for message in websocket:
                self.rxqueue.put( (websocket, message) )   # Possible improvement : Handle queue full scenario.
        except websockets.exceptions.ConnectionClosedError:
            pass
        finally:
            if self.disconnect_callback is not None:
                self.disconnect_callback(websocket)

    def process_tx_queue(self):
        while not self.txqueue.empty():
            (websocket, message) = self.txqueue.get()
            try:
                self.loop.run_until_complete(websocket.send(message))
            except websockets.exceptions.ConnectionClosedOK:
                pass    # Client is disconnected. Disconnect callback not called yet.

    def process(self, nloop=3) -> None:
        self.process_tx_queue()
        for i in range(nloop):  # Process events few times to make sure we handles events generated within the loop
            self.loop.call_soon(self.loop.stop)
            self.loop.run_forever()

    def start(self, host, port) -> None:
        # Warning. websockets source code says that loop argument might be deprecated. 
        self.ws_server = websockets.serve(self.server_routine, host, port, loop=self.loop)
        self.loop.run_until_complete(self.ws_server)    # Initialize websockets async server

    def stop(self) -> None:
        if self.ws_server is not None:
            self.ws_server.ws_server.close()
            self.loop.run_until_complete(asyncio.ensure_future(self.ws_server.ws_server.wait_closed(), loop=self.loop))
            self.loop.stop()
    
clients = set()
def connect_callback(websocket):
    clients.add(websocket)
    print('New client. Websocket ID = %s. We now have %d clients' % (id(websocket), len(clients)))

def diconnect_callback(websocket):
    clients.remove(websocket)
    print('Client diconnected. Websocket ID = %s. %d clients remaining' % (id(websocket), len(clients)))

def broadcast(message):      
    for websocketClient in clients:
        print("Sending: ")
        server.txqueue.put((websocketClient, message))
        
def init():
    global server
    server = SynchronousWebsocketServer(connect_callback=connect_callback, disconnect_callback=diconnect_callback)
    print("Starting server")
    server.start('localhost', 8000)
    print("Server started")
    
def run():
    try:
        server.process()
        if not server.rxqueue.empty():
            websocket, message = server.rxqueue.get_nowait()   # Non-blocking read. We need to keep call "server.process()" 
            print("Received message from websocket ID=%s. Echoing %s " % (id(websocket), message))
            server.txqueue.put((websocket, message))    # echo   
        time.sleep(0.005)
    except KeyboardInterrupt:
        exit()
