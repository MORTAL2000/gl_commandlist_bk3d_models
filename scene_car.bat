@echo off
echo WARNING: you should download additional models for this script to work
echo uncheck MODELS_DOWNLOAD_DISABLED in cmake and be patient
if exist bin_x64\Release (
    pushd bin_x64\Release
) else (
  if exist ..\bin_x64\Release (
    pushd ..\bin_x64\Release
  ) else goto failed
)
gl_commandlist_bk3d_models.exe -i scene_car.txt -a 1
popd
goto done
:failed
echo Failed to find the executable
:done
echo Finished
