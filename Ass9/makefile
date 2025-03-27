run: demandpaging.c
	gcc -Wall -o runsearch demandpaging.c
	./runsearch
vrun: demandpaging.c
	gcc -Wall -DVERBOSE -o runsearch demandpaging.c
	./runsearch
db: gensearch.c
	gcc -Wall -o gensearch gensearch.c
	./gensearch
clean:
	-rm -f runsearch gensearch