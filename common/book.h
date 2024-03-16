#ifndef BOOK_H
#define BOOK_H

#include "pch.h"
#include "market_data.h"

namespace common {

	class Book {
	public:
		typedef std::chrono::nanoseconds timestamp_t;

		Book();

		void set_precision(uint32_t precision);

		std::pair<double, double> best_bid() const;

		std::pair<double, double> best_ask() const;

		TopOfBook topOfBook(const std::string& symbol) const;

		double spread() const;

		double vwap(double price_level, bool is_bid) const;

		double vwap_mid(double bid_level, double ask_level) const;

		void update_book(double p, double a, bool is_bid);

		void clear_book();

		bool is_crossing() const;

		uint32_t scale(double price) const;

		double unscale(uint32_t price) const;

		std::string to_string(int levels, const std::string& pre) const;

	protected:
		typedef std::map<uint32_t, double, std::function<bool(const uint32_t&, const uint32_t&)>> order_book_snapshot_t;

		std::chrono::nanoseconds timestamp{};
		bool initialized{ false };
		order_book_snapshot_t bids;
		order_book_snapshot_t asks;
		uint32_t precision{ 10 };
	};

}

#endif 
