CC=g++
EXE=a2w22
OBJ = a2w22.o
CFLAGS = -std=c++11

%.o: %.cpp
	$(CC) -c -o $@ $< $(CFLAGS)

$(EXE): $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS)

.PHONY: clean tar
clean:
	rm -f $(OBJ) $(EXE)
