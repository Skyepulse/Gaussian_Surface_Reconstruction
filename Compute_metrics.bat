@ECHO OFF

REM Compute_metrics.bat generates all metrics for a Mesh folder

REM Usage: Compute_metrics.bat [Mesh_Folder] [Output_Folder]

REM Fetch input arguments

SET Mesh_Folder=%1
SET Output_Folder=%2

REM Check executable existence
IF NOT EXIST ".\off_polyscope\build\Debug\off_viewer.exe" (
    ECHO Error: Executable off_viewer.exe not found in .\off_polyscope\build\Debug\
    EXIT /B 1
)

REM Check if input arguments are provided
IF "%Mesh_Folder%"=="" (
    ECHO Error: Mesh_Folder argument is missing.
    ECHO Usage: Compute_metrics.bat [Mesh_Folder] [Output_Folder]
    EXIT /B 1
)

IF "%Output_Folder%"=="" (
    ECHO Error: Output_Folder argument is missing.
    ECHO Usage: Compute_metrics.bat [Mesh_Folder] [Output_Folder]
    EXIT /B 1
)

REM Create output folder if it doesn't exist
IF NOT EXIST "%Output_Folder%" (
    MKDIR "%Output_Folder%"
)

REM Check if the Mesh_Folder exists
IF NOT EXIST "%Mesh_Folder%" (
    ECHO Error: Mesh_Folder "%Mesh_Folder%" does not exist.
    EXIT /B 1
)

REM Check that the mesh folder contains the following folders: milo, sugar, triangle, triangle_2
IF NOT EXIST "%Mesh_Folder%\milo" (
    ECHO Error: Subfolder "milo" not found in "%Mesh_Folder%".
    EXIT /B 1
)
IF NOT EXIST "%Mesh_Folder%\sugar" (
    ECHO Error: Subfolder "sugar" not found in "%Mesh_Folder%".
    EXIT /B 1
)
IF NOT EXIST "%Mesh_Folder%\triangle" (
    ECHO Error: Subfolder "triangle" not found in "%Mesh_Folder%".
    EXIT /B 1
)
IF NOT EXIST "%Mesh_Folder%\triangle_2" (
    ECHO Error: Subfolder "triangle_2" not found in "%Mesh_Folder%".
    EXIT /B 1
)

REM for each subfolder, take all .off, .ply, .obj files and run off_polyscope/build/Debug/off_viewer.exe with arguments [mesh_file] [output_metrics_file]
FOR %%S IN (milo sugar triangle triangle_2) DO (
    FOR %%F IN ("%Mesh_Folder%\%%S\*.off" "%Mesh_Folder%\%%S\*.ply" "%Mesh_Folder%\%%S\*.obj") DO (
        SET "Mesh_File=%%F"
        SET "File_Name=%%~nF"
        SET "Output_Metrics_Folder=%Output_Folder%\%%S\%%~nF_metrics"
        
        REM Create output subfolder if it doesn't exist
        IF NOT EXIST "%Output_Folder%\%%S" (
            MKDIR "%Output_Folder%\%%S"
        )

        REM CHECK IF THE NAME OF THE FILE IS point_cloud.ply, if yes skip it
        IF "%%~nF"=="point_cloud" (
            ECHO Skipping "%%F" as it is named point_cloud.ply
            CONTINUE
        )

        REM Skip if metrics folder already exists
        IF EXIST "!OutputMetricsFolder!" (
            ECHO Skipping "%%F" as metrics folder "!OutputMetricsFolder!" already exists.
        ) ELSE (
            ECHO Processing "%%F"...
            CALL "%EXEC%" "%%F" "!OutputMetricsFolder!"
        )
    )
)

ECHO All metrics computed and saved in "%Output_Folder%".
ECHO Now running python script to generate plots...

ython scripts\output_metrics.py --input_path "%Output_Folder%"

IF ERRORLEVEL 1 (
    ECHO Error while running output_metrics.py
    EXIT /B 1
)

ECHO Plots generated successfully. Everything contained in Metrics"%Output_Folder%".
ENDLOCAL