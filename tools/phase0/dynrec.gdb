# Diagnostic for pinned DOSBox-X 4f19017c5f565dc40d01fede0f1382892e7243d7.
# Line 325 is the switch after core_dynrec.runcode returns a translated block.
# Recheck this breakpoint when upgrading DOSBox-X. Requires debug symbols.
set pagination off
set confirm off
set breakpoint pending off
handle SIGSEGV nostop noprint pass
tbreak CPU_Core_Dynrec_Run
commands
  silent
  printf "ARMDOS_DYNREC_ENTRY_REACHED\n"
  printf "ARMDOS_DYNREC_DECODER_MATCH=%d\n", (cpudecoder == &CPU_Core_Dynrec_Run)
  continue
end
tbreak core_dynrec.cpp:325
commands
  silent
  printf "ARMDOS_DYNREC_BLOCK_RETURN_REACHED\n"
  # The translated block can change cpudecoder (for example, guest HLT).
  # Check selection on entry; record the post-block decoder as context.
  print cpudecoder
  bt 1
  continue
end
run
