/* Override the audsrv_irx entry in libps2_drivers with the bundled fixed server. */
.section .rodata
.balign 64
.global audsrv_irx
.global size_audsrv_irx
audsrv_irx:
.incbin "ps2/iop/audsrv/audsrv.irx"
audsrv_irx_end:
.balign 4
size_audsrv_irx:
.word audsrv_irx_end - audsrv_irx
