CC = g++
FLAGS = -std=c++11 -Wall -O3 -no-pie
TARGET = train guess
all: $(TARGET)

train: transfer_learning_train.cpp
	g++ transfer_learning_train.cpp -o $@ $(FLAGS)

guess: transfer_learning_guess.cpp
	g++ transfer_learning_guess.cpp -o $@ $(FLAGS)

.PHONY: clean
clean:
	rm -f train
	rm -f guess
	rm -f *.o
