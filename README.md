# Zorro FIX Plugin

This project provides plugins for [Zorro](https://zorro-project.com/) to connect to brokers and simulators via the 
[FIX (version 4.4)](https://www.fixtrading.org/) API. Currently supported are: 
  
  - FXCM FIX Plugin: connecting to FXCM via [FXCM FIX API](https://github.com/fxcm/FIXAPI)
  - FIX Simulator Plugin: connecting to a market data simulator and matching engine via FIX

The project depends on the [QuickFix](https://quickfixengine.org) open source library.
Performance-wise it is not as fast as commercial FIX implementations but provides a straightforward
API and application framework to develop FIX based server and client applications. 
Some of the core building blocks and `fix_sumulation_server` are inspired from their examples. 

Contributions and feedback are very welcome. 


## Version History

  - v1.0.1 Improving order cancellation and many smaller bug fixing
  - v1.0.0 First official release
  - v2.0.0 First FXCM FIX plugin, separated and updated Simulation FIX plugin 



## FXCM FIX Plugin  

### Information

  - [FIX specification](https://apiwiki.fxcorporate.com/api/fix/docs/FXCM-FIX-BSI.pdf)
  - [FXCM data dictionary](https://apiwiki.fxcorporate.com/api/fix/docs/FIXFXCM10.xml)
  - [FIX conformance test](https://apiwiki.fxcorporate.com/api/fix/Retail_FIX_Conformance_Test.xlsx)
  - [FXCM FIX API examples](https://github.com/fxcm/FIXAPI)
  - [FXCM ForexConnect API](https://github.com/fxcm/ForexConnectAPI)
  - [ForexConnect online documentation](https://fxcodebase.com/wiki/index.php/Main_Page)
  - [ForexConnect online documentation](https://fxcodebase.com/bin/forexconnect/1.4.1/help/CPlusPlus/web-content.html#index.html)
  - [FXCM market data](https://github.com/fxcm/MarketData)


## Simulation FIX Plugin 

The goal of the FIX Simulator plugin is to provide a market data simulator and connect to it via FIX. 
Currently the the following standard FIX functionality is implemented:

  - Subscribe to (simulated) market data
    - MarketDataRequest (out)
	- MarketDataSnapshotFullRefresh (in)
	- MarketDataIncrementalRefresh (in)
  - Order execution
    - NewOrderSingle (out)
    - OrderCancelRequest (out)
    - OrderCancelReplaceRequest (out)
    - ExecReport (in) 

### TODO

Neither the Simulation FIX client plugin nor the FIX simulation server are yet complete. More work is required, current tasks on the roadmap are:

  - [fix_sumulation_server]: cancel automatically market data subscription on logout and test multiple connects from `zorro_fix_plugin`.
  - [fix_sumulation_server]: account functionality to support `BrokerAccount`
  - [fix_sumulation_server]: support for position (including asset balances) and trade reports, both, snapshot and updates
  - [fix_sumulation_server]: support for secruity list  
  - [fix_sumulation_server]: support order status request and mass order status request  
  - [zorro_fix_plugin]: implement function `BrokerAccount` for example with position snapshot and updates 
  - [zorro_fix_plugin]: integrate position update and trade capture report  
  - [zorro_fix_plugin]: integrate order status request repsoneses  
  - [zorro_fix_plugin]: more testing

Currently the market simulators only handle top of book. Eventually we want a fully simulated book, e.g. based in the `Fodra-Pham` model 
or any other suitable order book simulation model. 


### FIX Simulation Server

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

To make the development process more efficiently, the post build steps are added to the projects. Check the project properties for details.


## Build Issues

There is a build issue with spdlog on Windows reported on [GitHub](https://github.com/gabime/spdlog/issues/3042).
It is addressed by adding 

```
#define _SILENCE_ALL_MS_EXT_DEPRECATION_WARNINGS
``` 

to the `pch.h` files where needed.


