SOURCES = $(wildcard *.dot)
OBJECTS = $(SOURCES:.dot=.pdf)
OBJPGS = $(SOURCES:.dot=.jpg)

all: $(OBJECTS) $(OBJPGS) Makefile

%.pdf: %.dot
	dot -Tpdf $< -o $@.tmp
	pdfcrop $@.tmp $@
	rm $@.tmp

%.jpg: %.dot
	dot -Tjpg $< -o $@

clean:
	rm $(OBJECTS) $(OBJPGS)
