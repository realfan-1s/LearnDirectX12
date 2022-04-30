@echo off
set vscso=_vs.cso
set pscso=_ps.cso
set cscso=_cs.cso
set gscso=_gs.cso
set mscso=_ms.cso
set str=

for /r %%i in (*.hlsl) do (
    setlocal enabledelayedexpansion
    set error0=1
    findstr /R /i /c:"  *Vert *(" %%i > nul&&set "error0="
    if not defined error0 (
        fxc %%i /T vs_5_1 /E "Vert" /Fo %%~ni%vscso%
    )

    set error1=1
    findstr /R /i /c:"  *Frag *(" %%i > nul&&set "error1="
    if not defined error1 (
        fxc %%i /T ps_5_1 /E "Frag" /Fo %%~ni%pscso%
    )

    for /f "tokens=1* delims=:" %%j in ('findstr /n /i /r /c:" *numthreads *(" %%i') do (
        set /a line=%%j+1
        for /f "tokens=1* delims=:" %%n in ('findstr /i /r /c:".*" %%i') do (
            for /f "tokens=1* delims=:" %%k in ('findstr /n /l /c:"%%n" %%i') do (
                if !line!==%%k (
                    set str=%%n
                    for /f "tokens=1 delims=(" %%a in ("!str:~5%!") do (
                        fxc %%i /T cs_5_1 /E %%a /Fo %%~ni_%%~na%cscso%
                    )
                )
            )
        )
    )

    set error3=1
    findstr /R /i /c:"  *Geom *(" %%i > nul&&set "error3="
    if not defined error3 (
        fxc %%i /T gs_5_1 /E "Geom" /Fo %%~ni%gscso%
    )

    set error4=1
    findstr /R /i /c:"  *Mesh *(" %%i > nul&&set "error4="
    if not defined error4 (
        dxc -E "Mesh" -T ms_6_5 -Fo %%~ni%mscso% %%i
    )
)