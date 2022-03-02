CC=g++
EXE=a2w22
OBJ = a2w22.o
CFLAGS = -std=c++11
FILES_TO_TAR = makefile a2w22.cpp projectReport.pdf test.dat

%.o: %.cpp 
	$(CC) -c -o $@ $< $(CFLAGS)

$(EXE): $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS)

.PHONY: clean tar
clean:
	rm -f $(OBJ) $(EXE)
tar:
	tar -cvf CMPUT379-Ass2-Hdesmara.tar $(FILES_TO_TAR)