LATEX := latex -halt-on-error -interaction=nonstopmode -output-format=pdf
SNIPPETS := \
	tmp/vhdl/registers.tex \
	tmp/vhdl/fifos.tex \
	tmp/verilog/registers.tex \
	tmp/verilog/fifos.tex \
	tmp/cli/udev.tex \
	tmp/cli/flcli_help.tex \
	tmp/cli/comm_cksum.tex \
	tmp/cli/prog_cksum_vhdl.tex \
	tmp/cli/prog_fifo_vhdl.tex \
	tmp/cli/make_hdl_win_vhdl.tex \
	tmp/cli/make_hdl_lin_vhdl.tex \
	tmp/cli/prog_cksum_verilog.tex \
	tmp/cli/prog_fifo_verilog.tex \
	tmp/cli/make_hdl_win_verilog.tex \
	tmp/cli/make_hdl_lin_verilog.tex \
	tmp/c/check_macro.tex
PDFS := \
	images/by-sa.pdf \
	images/fpgalinkOverview.pdf \
	images/comm.pdf \
	images/jtag.pdf \
	images/commFpgaAppInterface.pdf \
	images/fx2allHarness.pdf \
	images/fx2minHarness.pdf \
	images/eppHarness.pdf \
	images/fx2Full.pdf \
	images/fx2Minimal.pdf \
	images/epp.pdf

all: paper.pdf mobile.pdf

#MAKESTUFF_TODAY_DATE := $(shell date +%Y%m%d)
MAKESTUFF_TODAY_DATE := 20121216

paper.pdf: top_level.tex ../style.tex tmp/pygments.tex $(SNIPPETS) $(PDFS)
	sed 's/MAKESTUFF_GEOMETRY//g' ../style.tex > tmp/style.tex
	sed 's/MAKESTUFF_TODAY_DATE/$(MAKESTUFF_TODAY_DATE)/g' $< > tmp/paper.tex
	${LATEX} tmp/paper.tex
	cmp -s paper.toc tmp/paper.toc; if [ "$$?" != "0" ]; then echo "New TOC"; cp paper.toc tmp/paper.toc; rm -f $@; ${LATEX} tmp/paper.tex; fi
	mv $@ tmp/$@
	gs -dBATCH -dNOPAUSE -q -sDEVICE=pdfwrite -sOutputFile=$@ tmp/$@
	ls -la tmp/$@ $@

mobile.pdf: top_level.tex ../style.tex tmp/pygments.tex $(SNIPPETS) $(PDFS)
	sed 's/MAKESTUFF_GEOMETRY/, paperwidth=16.3cm, paperheight=25cm, top=0.5cm, left=0.5cm/g' ../style.tex > tmp/style.tex
	sed 's/MAKESTUFF_TODAY_DATE/$(shell date +%Y%m%d)/g' $< > tmp/mobile.tex
	${LATEX} tmp/mobile.tex
	cmp -s mobile.toc tmp/mobile.toc; if [ "$$?" != "0" ]; then echo "New TOC"; cp mobile.toc tmp/mobile.toc; rm -f $@; ${LATEX} tmp/mobile.tex; fi
	mv $@ tmp/$@
	gs -dBATCH -dNOPAUSE -q -sDEVICE=pdfwrite -sOutputFile=$@ tmp/$@
	ls -la tmp/$@ $@

tmp/pygments.tex:
	mkdir -p tmp
	touch tmp/foo.c
	/usr/local/bin/pygmentize -O full -f latex -o tmp/foo.tex tmp/foo.c
	cat tmp/foo.tex | perl -e 'undef $$/;$$file=<STDIN>;$$file=~s/\\documentclass/%\\documentclass/mgs;$$file=~s/\\usepackage\[utf-8\]{inputenc}/%\\usepackage\[utf-8\]{inputenc}/mgs;$$file=~m/^(.*?)makeatother/mgs;print "$${1}def\\PY\@tok\@hu\{\\let\\PY\@bf=\\textbf\\def\\PY\@tc##1\{\\textcolor[rgb]\{0.00,0.00,0.00\}\{##1\}\}\}\n\\def\\PY\@tok\@mc\{\\def\\PY\@tc##1\{\\textcolor[rgb]\{0.20,0.20,0.20\}\{##1\}\}\}\n\\makeatother\n";' > $@

tmp/vhdl/%.tex: code/vhdl/%.vhdl
	mkdir -p tmp/vhdl
	/usr/local/bin/pygmentize -f latex -P verboptions="frame=leftline,numbersep=4px,xleftmargin=11px" -O linenos=True -F whitespace:tabs=True,tabsize=4 -o $@ $<

tmp/verilog/%.tex: code/verilog/%.v
	mkdir -p tmp/verilog
	/usr/local/bin/pygmentize -f latex -P verboptions="frame=leftline,numbersep=4px,xleftmargin=11px" -O linenos=True -F whitespace:tabs=True,tabsize=4 -o $@ $<

tmp/c/%.tex: code/c/%.c
	mkdir -p tmp/c
	/usr/local/bin/pygmentize -f latex -P verboptions="frame=leftline,numbersep=4px,xleftmargin=11px" -O linenos=True -F whitespace:tabs=True,tabsize=4 -o $@ $<

tmp/cli/%_vhdl.tex: code/cli/%.txt
	mkdir -p tmp/cli
	echo '\begin{Verbatim}[commandchars=\\\{\}]' > $@
	cat $< | sed 's/\\/\\PYZbs/g' | perl -ane 'if(m/^(.*?)¬(.*?)$$/g){print"\\PY{mc}{$$1}\\PY{hu}{$$2}\n";}else{chomp;print"\\PY{mc}{$$_}\n";}' | sed 's/\$$/\\\$$/g' | sed 's/verilog/vhdl/g' >> $@
	echo '\end{Verbatim}' >> $@

tmp/cli/%_verilog.tex: code/cli/%.txt
	mkdir -p tmp/cli
	echo '\begin{Verbatim}[commandchars=\\\{\}]' > $@
	cat $< | sed 's/\\/\\PYZbs/g' | perl -ane 'if(m/^(.*?)¬(.*?)$$/g){print"\\PY{mc}{$$1}\\PY{hu}{$$2}\n";}else{chomp;print"\\PY{mc}{$$_}\n";}' | sed 's/\$$/\\\$$/g' >> $@
	echo '\end{Verbatim}' >> $@

tmp/cli/%.tex: code/cli/%.txt
	mkdir -p tmp/cli
	echo '\begin{Verbatim}[commandchars=\\\{\}]' > $@
	cat $< | sed 's/\\/\\PYZbs/g;s/{/\\PYZob/g;s/}/\\PYZcb/g;s/\\PYZ\(..\)/\\PYZ\1{}/g' | perl -ane 'if(m/^(.*?)¬(.*?)$$/g){print"\\PY{mc}{$$1}\\PY{hu}{$$2}\n";}else{chomp;print"\\PY{mc}{$$_}\n";}' | sed 's/\$$/\\\$$/g' >> $@
	echo '\end{Verbatim}' >> $@

images/%.pdf : images/%.svg
	inkscape -f $< -A $@

#images/%.pdf : images/%.eps
#	epstopdf $<

#cat /home/chris/makestuff/libs/libfpgalink/hdl/sync/vhdl/ex_cksum/top_level.vhdl | perl -e 'undef $/;$file=<STDIN>;if($file=~m/BEGIN_SNIPPET\(registers\)\n(.*?)\n\s*\-\-\s*END_SNIPPET/mgs){print"$1\n";}' > code/vhdl/registers.vhdl
#cat /home/chris/makestuff/libs/libfpgalink/hdl/sync/vhdl/ex_fifo/top_level.vhdl | perl -e 'undef $/;$file=<STDIN>;if($file=~m/BEGIN_SNIPPET\(fifos\)\n(.*?)\n\s*\-\-\s*END_SNIPPET/mgs){print"$1\n";}' > code/vhdl/fifos.vhdl

clean: FORCE
	rm -rf *.log *.aux *.pdf *.out *.toc tmp images/*.pdf images/*.pdf

FORCE:
