moly: molymain.c ../src/molysynth.c
	cc -Wall -I../src $^ -o $@

clean:
	rm -f moly *~ tmp.wav

test:
	moly ../wav/scale1.wav
	open tmp.wav
