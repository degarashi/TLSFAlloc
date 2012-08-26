CC		= g++
CPPFLAGS	= -masm=intel --std=c++0x -O0 -g
PROGRAM		= tlsf
SRC		= $(wildcard *.cpp)
OBJ		= $(patsubst %.cpp,%.o, $(SRC))
DEPEND		= $(patsubst %.cpp,%.depend,$(SRC))

.cpp.o:
		$(CC) -c $(CPPFLAGS) $<
$(PROGRAM):	$(OBJ)
		$(CC) $(OBJ) -o $@

%.depend:	%.cpp
		@set -e; $(CC) -MM $(CPPFLAGS) $< \
                | sed 's/\($*\)\.o[ :]*/\1.o $@ : /g' > $@; \
                [ -s $@ ] || rm -f $@
-include $(DEPEND)

.PHONY: clean depend
clean:
	rm -f *.o *~ *.depend $(PROGRAM)
	rm -rf html/