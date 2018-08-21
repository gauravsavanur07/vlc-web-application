# vlc-web-application
The following  repo is inactive and the live version is available at following link
https://code.videolan.org/videolan/vlmc

 install dependencies
npm install

serve with hot reload at localhost:8080
npm run dev

 build for production with minification
npm run build

 build for production and view the bundle analyzer report
npm run build --report

 run unit tests
npm run unit

 run e2e tests
npm run e2e

 run all tests
npm test

 lint
npm run lint

 lint autofix
npm run lint -- --fix
After install python3 PIP and virtualenv 
virtualenv flask
flask/env/pip install flask 
./app.py
VLMC

Although almost everything was completed for separating UI and logic in VLMC, I still
needed to do some development to it because we required a way to control it remotely.
This means, we needed a server that listens for the commands from the client, and calls
the appropriate method by "unpacking" the command parameters. Hence, I designed and 
developed two classes, ControlServer and Controller, that does this job. ControlServer
handles the "network jobs", such as starting a WebSocket server, handling authentication,
"packing" messages (that is, adding timestamp, message type, index etc. to the message),
and lastly, forwarding command requests to Controller. Also, ControlServer serves as
C++/Qt counterpart of the socket library mentioned in WebVLMC section. The class Controller,
does the job of unpacking the command parameters and calling the actual method by the
command type supplied in the message. It is also in charge of serializing various data
structures such as project settings, workflow and so on. Lastly, I handles the forwarding
of various signals (such as, "workflow length changed") to ControlServer, which will
in turn send this to the client.

Along the way, I also needed to make some changes/additions in/to various other parts of
the VLMC code; whose further details can be found in the commit history.

Also, here I should note why I chose to use WebSockets instead of HTTP (or sth else)
as the way of communication. I remind that due to current Web standards, we only have
the choices HTTP and WebSocket. First of all, there is no official HTTP server module
in Qt, which means that I would need to either implement one myself using QTcpSocket,
or use an external library. Secondly, some commands do not complete immediately.
Hence, we would need some kind of threading and synchronization to make the client
for a response until the commands is completed. This would have made the architecture
far more complicated, and harder to debug. Thirdly, we might send and receive many
messages between the instance and client. Furthermore, the success status of commands
is important for the client. Since the overhead in HTTP is more than WebSocket, that
using HTTP might have slightly increased lags. Lastly, although I could have come
up with another way (such as making the client wait for HTTP requests' response for
sometime), some features are easier to implement using "backend events", which
obviously calls for sockets.

VLMC-Maestro

Obviously, we need to start a VLMC instance for each WebVLMC user, and load that user's
project file in the instance. Also, we have to make sure that no one can access another
user's project and video files. This requires authentication & session management,
instance-user matching and file system sandboxing. As an initial step towards that, 
I designed and developed VLMC-Maestro, which is a Python Flask web app that has a
REST API. By sending a HTTP POST request to the address '/api/instance', the client 
can request an instance to be started for her. Maestro passes along a random port 
number, a secret token, client origin and client IP to the instance as command line 
arguments. In the production case, Maestro should also pass the correct project file
addres obtained by the session information. However, currently it passes the same 
project file, since it is not completed yet. As the response to the client, Maestro
sends the socket address of the instance and the secret token that the client should
when trying to connect to the instance. With this method, we guarentee that
the client-instance matching is secure. As the future development direction that
needs to be taken for the Maestro, I can name the session management and file system
sandboxing.

