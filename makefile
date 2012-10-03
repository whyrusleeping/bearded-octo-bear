CC=g++


all:
	$(CC) main.cpp webOt.cpp strExt.cpp -lSDLmain -lSDL -lSDL_net -std=gnu++0x -o webOt


