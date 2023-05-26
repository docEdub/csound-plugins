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
    kValue2[] init 150

    SValue1 = WSget_s(12345, "/test/1")
    printsk("SValue1 = %s\n", SValue1)
    
    kValue2 = WSget_k(12345, "/osc-ar/hands")
    ki = 0
    printsk("kValue = [");
    while (ki < lenarray:k(kValue2)) do
        if (ki > 0) then
            printsk(", ")
        endif
        printsk("%f", kValue2[ki])
        ki += 1
    od
    printsk("]\n")
endin

</CsInstruments>
<CsScore>

i1 0 z

</CsScore>
</CsoundSynthesizer>
