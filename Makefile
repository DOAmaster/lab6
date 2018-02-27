all: lab6 srtm

lab6: lab6.cpp
	g++ lab6.cpp -Wall -lX11 -lGL -lGLU -lm ./libggfonts.a -o lab6

srtm: srtm.cpp
	g++ srtm.cpp -Wall -o srtm

clean:
	rm -f lab6 srtm
	rm -f *.o

