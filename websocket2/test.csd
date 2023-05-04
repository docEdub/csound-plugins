<CsoundSynthesizer>
<CsOptions>
-n -d -+rtmidi=NULL -M0 --midi-key-cps=4 --midi-velocity-amp=5
</CsOptions>
<CsInstruments>

nchnls = 1
sr = 48000
kr = 1
0dbfs = 1


opcode intFromString_k, k, S
    Svalue xin

    kgoto skip_i_time
    Svalue init "0"
    skip_i_time:

    kValue = strtolk(Svalue)
    xout kValue
endop

instr 1
    SValue1 init ""
    SValue2 init ""
    SValue1 = WSget(12345, "/test/1")
    SValue2 = WSget(12345, "/test/2")
endin

</CsInstruments>
<CsScore>

i1 0 z

</CsScore>
</CsoundSynthesizer>
