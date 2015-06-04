#
# Names of all subfolders; there is a one-to-one correspondence between these
# names and the names of the executables that will be built
#
BENCHES = bayes genome intruder kmeans labyrinth ssca2 vacation yada

#
# Target Info
#
.PHONY: all clean $(BENCHES)

#
# Targets
#
all: $(BENCHES)

clean:
	rm -rf obj32 obj64

$(BENCHES):
	$(MAKE) -C $@ all
