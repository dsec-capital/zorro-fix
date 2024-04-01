# Zorro FIX Plugin

This project provides a [Zorro](https://zorro-project.com/) plugin for FIX. 

It is still work in progress. A large part of the code is there but it lacks testing and is not yet fully 
functional. 


## Quick Start Tutorial

The `fix_sumulation_server` implements FIX 4.4 protocol for order execution against a model driven market.
It also provides a REST server to get historical market data, simulated from the model. 

Before starting `Zorro` the simulation server must be started:

```
fix_simulation_server.exe session.cfg market_config.toml
```


Then `Zorro` can be started. The plugin name is `_FixPlugin`. 



## Building QuickFix

QuickFix can be built from source directly with Visual Studio 2022. The debug and release QuickFix libraries should be 
copied into `third-parties\quickfix\x86-Debug` respectively `third-parties\quickfix\x86-Release`. Check also the 
project configurations where the QuickFix libraries are expecrted to be found. 


## Configuration of Post Build Steps in Visual Studio

First make sure that the environment variable `ZorroInstallDir` is set and points to the `Zorro` install location. 

Add the following to the post build steps.
For project `zorro_fix_plugin`:

```
echo %ZorroInstallDir%
copy /y "$(SolutionDir)$(Configuration)\zorrofixplugin.dll" "%ZorroInstallDir%\Plugin\_FixPlugin.dll"
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


## Overview



## Build

