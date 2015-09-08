SRC=$(wildcard *.c)
hmc5883l:
	gcc -std=gnu99 -o $@ ${SRC} -lm

clean:
	rm hmc5883l
