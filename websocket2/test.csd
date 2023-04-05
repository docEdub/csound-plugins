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


; instr 1
;     kValue = websocket2Get_k("/test")
;     printsk("kValue: %f\n", kValue)
; endin


; instr 2
;     kValue[] init 3
;     kValue = websocket2GetArray_k("/test")

;     printsk("kValue:\n")

;     ki = 0
;     while ki < lenarray(kValue) do
;         printsk("    [%d]: %f\n", ki, kValue[ki])
;         ki += 1
;     od
; endin

instr 3
    SValue init ""
    SValue = WSget("test", 12345)

    printsk(SValue)
    printsk("\n")
endin

</CsInstruments>
<CsScore>

;i 3 0 15
;i 3 15 15
i 3 0 z

</CsScore>
</CsoundSynthesizer>