all: $(TOP_LEVEL).svf

$(TOP_LEVEL).qsf: ../../platforms/$(PLATFORM)/platform.qsf $(HDLS)
	cp $< $@
	for i in $+; do if [ "$${i##*.}" = "vhdl" ]; then echo "set_global_assignment -name VHDL_FILE $$i"; elif [ "$${i##*.}" = "v" ]; then echo "set_global_assignment -name VERILOG_FILE $$i"; fi; done >> $@

$(TOP_LEVEL).svf: $(TOP_LEVEL).qsf $(TOP_LEVEL).fit.rpt
	quartus_asm --read_settings_files=on --write_settings_files=off $(TOP_LEVEL) -c $(TOP_LEVEL)

$(TOP_LEVEL).fit.rpt: $(TOP_LEVEL).map.rpt
	quartus_fit --parallel=1 --read_settings_files=on --write_settings_files=off $(TOP_LEVEL) -c $(TOP_LEVEL)

$(TOP_LEVEL).sdc: ../../platforms/$(PLATFORM)/platform.sdc
	cp $< $@

$(TOP_LEVEL).map.rpt: $(TOP_LEVEL).vhdl $(TOP_LEVEL).sdc
	echo '{ "Warning" "WCPT_FEATURE_DISABLED_POST" "LogicLock " "Warning (292013): Feature LogicLock is only available with a valid subscription license. You can purchase a software subscription to gain full access to this feature." {  } {  } 0 292013 "Feature %1!s! is only available with a valid subscription license. You can purchase a software subscription to gain full access to this feature." 1 0 "" 0 -1}' > $(TOP_LEVEL).srf
	quartus_map --parallel=1 --read_settings_files=on --write_settings_files=off $(TOP_LEVEL) -c $(TOP_LEVEL)

clean: FORCE
	rm -rf *.qsf *.svf db incremental_db *.rpt *.summary *.pin *.qpf *.dpf *.smsg *.jdi *.pof *.sof *.done *.bak *.sdc *.srf

FORCE:
