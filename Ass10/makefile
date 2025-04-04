run: LRU.c
	gcc -Wall -o runsearch LRU.c
	./runsearch
vrun: LRU.c
	gcc -Wall -DVERBOSE -o runsearch LRU.c
	./runsearch > output.txt
db: gensearch.c
	gcc -Wall -o gensearch gensearch.c
	./gensearch
clean:
	-rm -f runsearch gensearch