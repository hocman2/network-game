## What is this ?

https://github.com/user-attachments/assets/a94cb271-3aa8-49e2-b44f-91e79ddae27c

It's a small demo of how a multiplayer game can be achieved in C++. It doesn't use any remote server, rather a player is a designated host while others are client.
It uses Raylib for the graphical part otherwise the C/C++ standard libraries

The Host controls the red ship (shown grey on clients) while clients can spawn balls under their mouse cursor by hitting space.
This basic project demonstrates client/server interactions on both sides and how to synchronize between both. 

## Build
Using CMake/make

```
mkdir build
cd build
cmake ..
make
```

Untested and unconfigured for other IDEs (Xcode, VS, etc.)
Raylib is self contained in the repository for simplicity.

## How to use
Once built, start the *Net* executable with the `-h` command line argument to start a host instance.
Then start as many *Net* instances as desired.

Host session shows the triangle in red, client sessions are able to spawn balls by hitting space.

I didn't implement any logic for a client to join the host if it's created first so a host session must be started first.

**This only works on localhost**

## Project structure
The game code lives in *game.cc* and the host/client logic is cluttered together, I'll agree it's not ideal for readability but this is a weekend project.
I tried to keep the code running in the net thread inside *net.cc*, this is where socket binding/message sending is done. There is some overlap with game logic obviously but I tried to keep it minimal

## Known issues
- Under certain angle, the interpolation on client side messes up for the triangle, i didn't bother fixing it.
- Shutting down a client session also shuts down the host session. This is undesirable and, as a consequence, logic for when a client leaves is untested.
- Overall the project lacks safety and error checking making it quite fragile.

## Compatibility
This project uses UNIX socket and thus, does not work on Windows.
