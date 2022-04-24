@echo off
set vscso=_vs.cso
set vsasm=_vs.asm
set pscso=_ps.cso
set psasm=_ps.asm
set cscso=_cs.cso
set csasm=_cs.asm
set gscso=_gs.cso
set gsasm=_gs.asm
set mscso=_ms.cso
set msasm=_ms.asm
for /r %%i in (*.hlsl) do (
    set error0=1
    findstr /i " Vert(" %%i > nul&&set "error0="
    if not defined error0 (
        
        fxc %%i /T vs_5_1 /E "Vert" /Fo %%~ni%vscso% /Fc %%~ni%vsasm%
    )
    set error1=1
    findstr /i " Frag(" %%i > nul&&set "error1="
    if not defined error1 (
        fxc %%i /T ps_5_1 /E "Frag" /Fo %%~ni%pscso% /Fc %%~ni%psasm%
    )
    set error2=1
    findstr /i " Compute(" %%i > nul&&set "error2="
    if not defined error2 (
        fxc %%i /T cs_5_1 /E "Compute" /Fo %%~ni%cscso% /Fc %%~ni%csasm%
    )
    set error3=1
    findstr /i " Geom(" %%i > nul&&set "error3="
    if not defined error2 (
        fxc %%i /T gs_5_1 /E "Geom" /Fo %%~ni%gscso% /Fc %%~ni%gsasm%
    )
)