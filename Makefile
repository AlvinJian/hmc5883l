SRC=$(wildcard *.c)
hmc5883l:
	gcc -std=c99 -o $@ ${SRC}

clean:
	rm *.o hmc5883l
