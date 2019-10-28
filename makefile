TARGET = train guess
all: $(TARGET)

train: transfer_learning_train.cpp
	g++ transfer_learning_train.cpp -o train -O2 -no-pie

guess: transfer_learning_guess.cpp
	g++ transfer_learning_guess.cpp -o guess -O2 -std=c++11 -no-pie

.PHONY: clean
clean:
	rm -f train
	rm -f guess
	rm -f *.o
