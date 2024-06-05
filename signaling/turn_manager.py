from twilio.rest import Client

class Turn_manager():
    def __init__(self,SID,token):
        self._client = Client(SID,token)        
    
    @property
    def servers(self):
        token = self._client.tokens.create()
        return token.ice_servers
