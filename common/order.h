#ifndef ORDER_H
#define ORDER_H

#include <string>
#include <iomanip>
#include <ostream>

namespace common {

	class Order
	{
		friend std::ostream& operator<<(std::ostream&, const Order&);

	public:
		enum Side { buy, sell };
		enum Type { market, limit };

		Order(
			const std::string& clOrdId,
			const std::string& symbol,
			const std::string& owner,
			const std::string& target,
			Side side,
			Type type,
			double price,
			long quantity
		);

		const std::string& get_client_id() const;
		const std::string& get_symbol() const;
		const std::string& get_owner() const;
		const std::string& get_target() const;
		Side get_side() const;
		Type getType() const;
		double get_price() const;
		long get_quantity() const;

		long get_open_quantity() const;
		long get_executed_quantity() const;
		double get_avg_executed_price() const;
		double get_last_executed_price() const;
		long get_last_executed_quantity() const;

		bool isFilled() const;
		bool isClosed() const;

		void execute(double price, long quantity);

		void cancel();

		std::string to_string() const;

	private:
		std::string cl_ord_id;
		std::string symbol;
		std::string owner;
		std::string target;
		Side side;
		Type type;
		double price;
		long quantity;
		long open_quantity;
		long executed_quantity;
		double avg_executed_price;
		double last_executed_price;
		long last_executed_quantity;
	};

	std::ostream& operator<<(std::ostream&, const Order&);
}

#endif
