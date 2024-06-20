# Zorro FIX Plugin

This project provides plugins for [Zorro](https://zorro-project.com/) to connect to brokers and simulators via the 
[FIX (version 4.4)](https://www.fixtrading.org/) API. Currently supported are: 
  
  - FXCM FIX Plugin: connecting to FXCM via [FXCM FIX API](https://github.com/fxcm/FIXAPI) and pulling historical 
    market data with [FXCM ForexConnect SDK](https://fxcodebase.com/wiki/index.php/Download)
  - FIX Simulator Plugin: connecting to a market data simulator and matching engine via FIX

The project depends on the [QuickFix](https://quickfixengine.org) open source library.
Performance-wise it is not as fast as commercial FIX implementations but provides a straightforward
API and application framework to develop FIX based server and client applications. 
Some of the core building blocks and [fix_simulation_server](fix_simulation_server) are inspired from their examples. 

Contributions, bug reports and constructive feedback are very welcome. 



## Version History

  - v2.1.0 FXCM ForexConnect is accessed through a proxy server, smaller bug fixes, order status messages, 64 bit version 
  - v2.0.0 First FXCM FIX plugin, separated and updated Simulation FIX plugin 
  - v1.0.1 Improving order cancellation and many smaller bug fixing
  - v1.0.0 First official release



## Dependencies

The project uses most modern [C++ 20](https://en.cppreference.com/w/cpp/20) and depends on the following 
header only third party components:

  - [httplib ](third_parties/httplib)
  - [nlohmann json](third_parties/nlohmann)
  - [toml++](third_parties/toml++)
  - [magic_enum](third_parties/magic_enum)
  - [spdlog](third_parties/spdlog)


### Library Dependencies

  - [QuickFix](third_parties/quickfix) with prebuilt static libraries included for x86 and x64
  - [FXCM ForexConnect SDK](third_parties/fxcm) which requires separate installation 
   - [x86](http://fxcodebase.com/bin/forexconnect/1.6.5/ForexConnectAPI-1.6.5-win32.exe)
   - [x64](http://fxcodebase.com/bin/forexconnect/1.6.5/ForexConnectAPI-1.6.5-win64.exe)


### Environtment Configuration

The environment variable `%ZorroInstallDir%` has to point to the Zorro installation 

```
echo %ZorroInstallDir%
C:\zorro\Zorro_2614
```


The FXCM market data server can log tick data and incremental bar updates. The log path 
can be configured by specifying the environment variable `FXCM_MAKRET_DATA_SERVER_LOG_PATH` 

```
echo %FXCM_MAKRET_DATA_SERVER_LOG_PATH%
C:\zorro\Zorro_2614\Log\fxcm_market_data_server_
```

The installation of FXCM ForexConnect SDK sets the following environment variables

```
echo %FOREXCONNECT_PATH_X64%
C:\Program Files\Candleworks\ForexConnectAPIx64
```

respectively

```
echo %FOREXCONNECT_PATH_X86%
C:\Program Files (x86)\Candleworks\ForexConnectAPI
```

Assure that they point to the installed version of the ForexConnect SDK.

In order to use the C++ test script from Zorro, add the path to the Visual Studio build directory to 
the `Zorro.ini` file:

```
VCPath = "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build" 
```


## FXCM FIX Plugin  

The FXCM FIX client plugin implements the FXCM FIX 4.4. protocol, inlcuding their specific extensions as documented
in the [FXCM FIX specification](https://apiwiki.fxcorporate.com/api/fix/docs/FXCM-FIX-BSI.pdf). 
The [FXCM ForexConnect SDK](https://fxcodebase.com/wiki/index.php/Download) is used to fetch historical market data from FXCM. 

Zorro already comes with a [plugin for FXCM](https://zorro-project.com/manual/en/fxcm.htm) which uses the  
[FXCM ForexConnect SDK](https://fxcodebase.com/wiki/index.php/Download) under the hood. The advantage of the FIX protocol
is that it offers much better rate limits as can be seen from the 
[FXCM API comparison](https://www.fxcm.com/markets/algorithmic-trading/compare-api/). 


### Supported Features

The FXCM FIX client plugin implements all of Zorro's [broker plugin functions](https://zorro-project.com/manual/en/brokerplugin.htm).

  - Historical market data through [FXCM ForexConnect SDK](https://fxcodebase.com/wiki/index.php/Download)
  - Subscribe to real time market data either as snapshot and updates or snapshot and incremental update.
    Noet that FXCM only provides top of book and generally no volume
  - NewOrderSingle supporting market and limit orders
  - Order cancellation and order modification 
  - Trading session status (message TradingSessionStatus)
  - Position reports (message PositionReport)
  - Collateral inquiry (message CollateralReport)
  - Execution reports (message ExecReport)

More details can be found in [FXCM FIX client application](zorro_fxcm_fix_plugin/application.h). 

## FXCM Market Data Proxy Server

From version 2.1.0 the FXCM ForexConnect API is not directly integrated anymore with the Zorro FIX plugin but is running as a 
standalone proxy server `fxcm_market_data_server`, exposing various rest endpoints. It can be directly started from Visual Studio 
and extends the system path with the respective path to the FXCM ForexConnect dlls:

Project Properties fxcm_market_data_server -> Configuration Properties -> Debugging -> Environment 
```
Path=$(Path);$(FOREXCONNECT_PATH_X64)\bin
```
for platform x64, respectively for x86 or Win32
```
Path=$(Path);$(FOREXCONNECT_PATH_X86)\bin
```

Start `fxcm_market_data_server` with the proper path setting

```
C:\repos\zorro-fix\x64\Debug>fxcm_market_data_server.exe
[2024-06-19 20:23:51.874] [combined] [debug] Logging started, logger_name=combined, level=1, cwd=C:\repos\zorro-fix\x64\Debug
[2024-06-19 20:23:51.899] [combined] [info] session status connecting
[2024-06-19 20:24:00.263] [combined] [info] session status connected
[2024-06-19 20:24:00.264] [combined] [info] connected - server ready to accept service requests
[2024-06-19 20:24:00.266] [combined] [info] server stated on 0.0.0.0:8080
```

It logs to the screen as well as to the log file `fxcm_proxy_server.log`.


### Installation

The FXCM FIX client plugin is built against ForexConnect SDK 1.6.5, which can be downloaded from these links:  

  - [ForexConnect SDK 1.6.5 Win32](http://fxcodebase.com/bin/forexconnect/1.6.5/ForexConnectAPI-1.6.5-win32.exe)
  - [ForexConnect SDK 1.6.5 Win64](http://fxcodebase.com/bin/forexconnect/1.6.5/ForexConnectAPI-1.6.5-win64.exe)

Note that the link libraries and headers are in the GitHub repository to facilitate the build process 
and can be found in the [FXCM third party directory](third-parties/fxcm/forex_connect). 
For another version of ForexConnect SDK the libraries and include files have to be changed accordingly. 

The FXCM FIX client plugin requires the FXCM ForexConnect SDK in the search path. Add the FXCM ForexConnect SDK 
`C:\Program Files (x86)\Candleworks\ForexConnectAPI\bin` directory to the system path. 

The project requires the following environment variables to be defined:

  - ZorroInstallDir: directory where Zorro is installed
  - FIX_ACCOUNT_ID: account id provided by FXCM
  - FIX_USER_NAME: user name provided by FXCM
  - FIX_PASSWORD: password provided by FXCM
  - FIX_TARGET_SUBID: target subid provided by FXCM

These environment variables are used in various automation scripts in the [scripts](scripts) folders 
as well as in Visual Studio post build events. 

Eventually it is necessary to update the [FIX session config template](zorro_fxcm_fix_plugin/zorro_fxcm_fix_client_template.cfg).
Note that this file is configured appropriately from the values of the environment variables and copied to the 
Zorro installation directory as part of the post build process. 


### TODO

The FXCM FIX client plugin is rather complete. The following tasks are on the road map:

  - Other order types beside market and limit orders, e.g. adding stop loss.
  - Build 64 bit version 
  - Order mass status report for open working orders
  - Position reports subscriptions for udpates  
  - Convenient Zorro script for FIX conformance testing with FXCM
  - Ability to pull FXCM historical tick data in `BrokerHistory2`
  - More testing


### References

  - [FIX specification](https://apiwiki.fxcorporate.com/api/fix/docs/FXCM-FIX-BSI.pdf)
  - [FXCM data dictionary](https://apiwiki.fxcorporate.com/api/fix/docs/FIXFXCM10.xml)
  - [FIX conformance test](https://apiwiki.fxcorporate.com/api/fix/Retail_FIX_Conformance_Test.xlsx)
  - [FXCM FIX API examples](https://github.com/fxcm/FIXAPI)
  - [FXCM ForexConnect API](https://github.com/fxcm/ForexConnectAPI)
  - [FXCM ForexConnect SDK](https://fxcodebase.com/wiki/index.php/Download)
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

The Simulation FIX client plugin and the FIX simulation server are work in progress. Current tasks on the roadmap are:

  - [fix_sumulation_server]: account functionality to support `BrokerAccount`
  - [fix_sumulation_server]: support for position (including asset balances) and trade reports, both, snapshot and updates
  - [fix_sumulation_server]: support for secruity list  
  - [fix_sumulation_server]: support order status request and mass order status request  
  - [zorro_sim_fix_plugin]: implement function `BrokerAccount` for example with position snapshot and updates 
  - [zorro_sim_fix_plugin]: integrate position update and trade capture report  
  - [zorro_sim_fix_plugin]: integrate order status request repsoneses  
  - [zorro_sim_fix_plugin]: more testing

The market simulators only handle top of book. Eventually we want a fully simulated book, e.g. based in the `Fodra-Pham` model 
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


