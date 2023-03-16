build:
	g++ -Wall -Werror tema1.cpp -o tema1 -lpthread

build_debug:
	g++ -Wall -Werror tema1.cpp -o tema1 -DDEBUG -g3 -O0 -lpthread

clean:
	rm tema1