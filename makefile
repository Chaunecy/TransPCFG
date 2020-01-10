CC = g++
FLAGS = -std=c++11 -Wall -O3 -no-pie
TARGET = train guess venv requirements
all: $(TARGET)

train: transfer_learning_train.cpp
	g++ transfer_learning_train.cpp -o $@ $(FLAGS)

guess: transfer_learning_guess.cpp
	g++ transfer_learning_guess.cpp -o $@ $(FLAGS)

venv: /usr/bin/python3
	python3 -m venv venv

requirements: requirements.txt
	source ./venv/bin/activate && pip install -r requirements.txt

.PHONY: clean
clean:
	rm -f train
	rm -f guess
	rm -f *.o
