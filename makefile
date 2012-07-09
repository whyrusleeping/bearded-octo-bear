all:
	g++ -std=gnu++0x main.cpp webOt.h webOt.cpp strExt.h strExt.cpp -lSDLmain -lSDL_net -o webOt