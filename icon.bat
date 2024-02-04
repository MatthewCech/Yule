@echo off

:: Download resource hacker 
IF NOT EXIST temp_tools/ (
  :: Ensure we have a working area
  mkdir temp_tools

  :: Download resource hacker, putting it in a temp_tools folder
  call curl https://angusj.com/resourcehacker/resource_hacker.zip -o ./temp_tools/resource_hacker_download.zip

  :: Unzip the downloaded file
  powershell Expand-Archive -Force ./temp_tools/resource_hacker_download.zip ./temp_tools/RH
)

:: Apply an icon to any executables present in build output folders, placing exe back in root directory
echo.
IF EXIST x64/Debug (
  call "temp_tools/RH/ResourceHacker.exe" -open "x64/Debug/Yule.exe" -save "x64/Debug/Yule.exe" - action addskip -res "Resources/icon-256-128-64.ico" -mask ICONGROUP,MAINICON,0

  IF NOT EXIST build/ (
    mkdir build
  )
  copy x64\Debug\Yule.exe .\build\Yule.exe
) ELSE ( 
  echo Directory does not exist. 
)

:: Update explorer icon cache
call ie4uinit.exe -ClearIconCache

:: Hold tool for half a minute in case there's some output we want to see.
timeout 30