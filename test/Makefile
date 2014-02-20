target = serial_test

all: serial_test

$(target): serial_test.c
	gcc -o serial_test serial_test.c -Xlinker -rpath=../lib -L ../lib -lmsr

clean:
	rm -f $(target)
