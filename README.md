# Zorro FIX Plugin

This project provides a [Zorro](https://zorro-project.com/) plugin for 
[FIX (version 4.4)](https://www.fixtrading.org/). 

The project is work in progress. More testing is required. Also only a limited set of the
standard FIX functionality is implemented:

  - Subscribe to (simulated) market data
    - MarketDataRequest (out)
	- MarketDataSnapshotFullRefresh (in)
	- MarketDataIncrementalRefresh (in)
  - Order execution
    - NewOrderSingle
    - OrderCancelRequest
    - OrderCancelReplaceRequest
    - ExecReport (in) 

The project depends on the [QuickFix](https://quickfixengine.org) open source library.
Performance-wise it is not as fast as commercial FIX implementations but provides a straightforward
API and application framework to develop FIX based server and client applications. 
Some of the core building blocks and `fix_sumulation_server` are inspired from their examples. 

Neither the FIX client plugin nor the FIX server are complete. Edge cases and errors and rejects are 
currently only handled in a rudimentary manner. 

Contributions and feedback are very welcome. 

## TODO

Some tasks on the roadmap:

  - [fix_sumulation_server]: cancel automatically market data subscription on logout and test multiple connects from `zorro_fix_plugin`.
  - [fix_sumulation_server]: better working order status log in command line
  - [fix_sumulation_server]: account functionality to support `BrokerAccount` 
  - [zorro_fix_plugin]: implement function `BrokerAccount` 
  - [zorro_fix_plugin]: better log output when cancelling orders in `BrokerBuy2`
  - [zorro_fix_plugin]: a lot of testing

Currenbtly the market simulators only handle top of book. Eventually we want a fully simulated book, e.g. based in the `Fodra-Pham` model 
or any other reasonable order book simulation model. 


## Quick Start Tutorial

### FIX Server

The `fix_sumulation_server` implements FIX 4.4 protocol for order execution against a model driven market.
It also provides a REST server to get historical market data, simulated from the model. 

Before starting `Zorro` the simulation server must be started:

```
fix_simulation_server.exe session.cfg market_config.toml
```

The FIX session configuration `session.cfg` is the first argument. The second argument is the
market configuration `market_config.toml`. These files are copied to the build directories 
with a post build event. 

### Zorro Plugin

Start `Zorro` as usual and check if there is a plugin with the name `_FixPlugin`. 
This can be used with the `Test_FIX.c` trading script. 

The FIX session configuration for the plugin is specified in `zorro_fix_client.cfg`. The file
will be copied to the `Zorro` plugin directory `%ZorroInstallDir%\Plugin` with a post build event.


## Online Resources

In order to parse a FIX message string there are several online parsers such as 

  - [Esprow](https://www.esprow.com/fixtools/parser.php)


## Building QuickFix

[QuickFix](https://github.com/quickfix/quickfix/) can be built from source directly with Visual Studio 2022. 

The debug and release QuickFix libraries should be copied into `third-parties\quickfix\x86-Debug` respectively 
`third-parties\quickfix\x86-Release`. 

Check also the project configurations where the QuickFix libraries are expecrted to be found. 


## Configuration of Post Build Steps in Visual Studio

First make sure that the environment variable `ZorroInstallDir` is set and points to the `Zorro` install location. 

To make the development process more efficiently, the following post build steps are added to the projects. 
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

There is a build issue with spdlog on Windows reported on [GitHub](https://github.com/gabime/spdlog/issues/3042).
It is addressed by adding 

```
#define _SILENCE_ALL_MS_EXT_DEPRECATION_WARNINGS
``` 

to the `pch.h` files where needed.


