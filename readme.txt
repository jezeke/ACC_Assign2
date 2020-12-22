How to Compile:

The client and server programs can be built using the make command:

Builds the client program:
  make client

Builds the server program:
  make server

Builds both the client and server programs:
  make all

Cleans up build artifacts and executables:
  make clean

How to Use:

The server can be executed like so: './server n m wait_time',
where n is the initial number of threads for handling client connections,
m is the number of threads added to the threadpool when it reaches capacity,
and wait_time is the number of seconds an idle client will remain before
being timed out.

Example: './server 10 4 120' to start the server with ten threads that will
gain four additional threads if nine clients are connected concurrently;
each of which will be disconnected if they do not send a command for more
than 120 seconds.

The client can be executed like so: './client ip port', where ip is the
hostname or IP address of the server, (IP addresses must be in IPv4
dot-decimal notation, and port is the network port that the server is running at.
Example: './client 192.168.1.234 1234' to connect to a server running at
192.168.1.234 on port 1234.
Example: './client localhost 52001' to connect to a server running on the
same machine as the client, on port 52001.

Once launched, commands can be input into the client. The following commands
are accepted, along with their expected arguments and a usage description:
  'JOIN username realname' where username and realname are the names you would
    like to join the server with.
  'NICK nickname' where nickname is the new nickname you would like to change to.
  'WHO' returns the list of users on the server.
  'WHOIS target' returns information on a user on the server.
  'TIME' returns the current system time.
  'PRIVMSG target text' where target is the person you would like to send the
    text to privately.
  'BCASTMSG text' where text is what you would like to broadcast to all users.
  'QUIT text' where text is what you would like to broadcast to all users
    before disconnecting.

Known Issues:

-User input is interspersed with messages from the server. With more time,
this could be corrected, however the priority was low and the work required
is quite high.

-User input is not interrupted by the connection being closed. It may
take an input or two for the program to close once the connection is closed.
