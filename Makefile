all: grader

grader: autograder_main.cpp fs.cpp disk.c
	g++ -w autograder_main.cpp disk.c fs.cpp -o grader

clean:
	rm grader
