OBJECTS=main.o levels.o util.o
EXE=runme
CC=clang
CFLAGS=-Wall -g -D 'BUILD_USER="$(USER)"'
RM=rm -f

$(EXE): $(OBJECTS)
	$(CC) $(CFLAGS) $(OBJECTS) -o $(EXE)
	chmod u+s $(EXE)

.PHONY: clean
clean:
	$(RM) $(EXE)
	$(RM) *.o
