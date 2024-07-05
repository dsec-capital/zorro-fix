#pragma once

// includes for Zorro Light-C / C++ 

// additional broker commands
#define BROKER_CMD_CREATE_ASSET_LIST_FILE						2000
#define BROKER_CMD_CREATE_SECURITY_INFO_FILE					2001

// updated COrderPositionArg struct
#define BROKER_CMD_GET_ORDER_POSITION_ID						2002		

#define BROKER_CMD_PRINT_ORDER_TRACKER							2010
#define BROKER_CMD_GET_ORDER_TRACKER_NUM_ORDER_REPORTS			2011
#define BROKER_CMD_GET_ORDER_TRACKER_ORDER_REPORTS				2012
#define BROKER_CMD_GET_ORDER_TRACKER_NUM_NET_POSITIONS			2013
#define BROKER_CMD_GET_ORDER_TRACKER_NETPOSITIONS				2014

// initiates order mass status report and returns the size, get it with BROKER_CMD_GET_ORDER_ORDER_MASS_STATUS
#define BROKER_CMD_GET_ORDER_ORDER_MASS_STATUS_SIZE				2015
#define BROKER_CMD_GET_ORDER_ORDER_MASS_STATUS					2016

// initiate a position report request, get it with BROKER_CMD_GET_POSITION_REPORT
#define BROKER_CMD_GET_OPEN_POSITION_REPORT_SIZE				2017
#define BROKER_CMD_GET_OPEN_POSITION_REPORT						2018

// initiate a position report request, get it with BROKER_CMD_GET_POSITION_REPORT
#define BROKER_CMD_GET_CLOSED_POSITION_REPORT_SIZE				2019
#define BROKER_CMD_GET_CLOSED_POSITION_REPORT					2020

#define BROKER_CMD_SET_CANCEL_REPLACE_LOT_AMOUNT				2021

typedef struct COrderPositionArg {
	int trade_id;				// input
	char position_id[1024];		// output
	int trade_not_found;
	int has_open_position;		 
} COrderPositionArg;

// OrderReport from order tracker (calculated via ExecReports)
typedef struct COrderReport {
	char symbol[256];
	char ord_id[1024];
	char cl_ord_id[1014];
	char ord_type;
	char ord_status;
	char side;
	double price;
	double avg_px;
	double order_qty;
	double cum_qty;
	double leaves_qty;
	char position_id[1024];
} COrderReport;

// NetPosition from order tracker (calculated via ExecReports)
typedef struct CNetPosition {
	char account[1024];
	char symbol[1024];
	double avg_px;
	double qty;
} CNetPosition;

// Order status report from order mass status request
typedef struct CStatusExecReport {
	char symbol[256];
	char ord_id[1024];
	char cl_ord_id[1014];
	char exec_id[1024];
	char mass_status_req_id[1024];
	char exec_type;
	char ord_type;
	char ord_status;
	char side;
	double price;
	double avg_px;
	double order_qty;
	double last_qty;
	double last_px;
	double cum_qty;
	double leaves_qty;
	char text[4096];
	int tot_num_reports;
	int last_rpt_requested;
	char position_id[1024];
} CStatusExecReport;

// Position report obtained via FIX 
typedef struct CFXCMPositionReport { 
	char account[1024];
	char symbol[1024];
	char currency[1014];
	char position_id[1024];

	double settle_price;

	int is_open;

	// valid for open and closed positions 
	double interest;
	double commission;
	double open_time;			// from nanoseconds timestamp utc converted to zorro time

	// valid only for open positions
	double used_margin;

	// valid only for closed positions
	double close_pnl;
	double close_settle_price;
	double close_time;			// nanoseconds timestamp utc converted to zorro time
	char close_order_id[1024];
	char close_cl_ord_id[1024];
} CFXCMPositionReport;
