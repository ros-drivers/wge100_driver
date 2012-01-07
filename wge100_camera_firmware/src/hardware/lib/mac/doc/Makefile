name=mac

srcs= mac.tex \
#

figs= mac-blocks.fig \
#

pdfs = $(figs:%.fig=figs/%.pdf)

.PHONY: default
default: all

.PHONY: all
all: $(name).pdf

%.pdf: %.fig
	fig2dev -L pdf $< $@ 

$(name).pdf: $(srcs) $(pdfs)
	@echo ----- pdflatex 1... -----
	pdflatex `basename $@ .pdf`.tex
	@echo ----- pdflatex 2... -----
	pdflatex `basename $@ .pdf`.tex
	@echo ----- done -----

.PHONY: clean
clean:
	rm -f *.aux *.bbl *.blg *.log $(name).pdf TAGS


