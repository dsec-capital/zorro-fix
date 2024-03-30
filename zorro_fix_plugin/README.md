# Zorro FIX Plugin

## Quick Start Tutorial

The `fix_sumulation_server` implements FIX 4.4 protocol for order execution against a model driven market.
It also provides a REST server to get historical market data, simulated from the model. 



## Configuration of Post Build Steps in Visual Studio

Add the following to the post build steps.
For project `zorro_fix_plugin`:

```
echo %ZorroInstallDir%
copy /y "$(SolutionDir)$(Configuration)\zorrofixplugin.dll" "%ZorroInstallDir%\Plugin\AAFixPlugin.dll"
copy /y "$(ProjectDir)zorro_fix_client.cfg" "$(SolutionDir)$(Configuration)"
copy /y "$(ProjectDir)zorro_fix_client.cfg" "%ZorroInstallDir%\Plugin"
robocopy "$(SolutionDir)spec" "%ZorroInstallDir%\Plugin\spec"  /MIR
``` 

For project `fix_simulation_server`:

```
copy /y "$(ProjectDir)session.cfg" "$(SolutionDir)$(Configuration)"
```

For project `common`:

```
copy /y "$(ProjectDir)market_config.toml" "$(SolutionDir)$(Configuration)"
``` 


## Build Issues

There is a build issue with spdlog on Windows reported on (GitHub)[https://github.com/gabime/spdlog/issues/3042].


