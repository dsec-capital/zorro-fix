#include "pch.h"

#include "order.h"

namespace common {

		Order::Order(
			const std::string& clOrdId,
			const std::string& symbol,
			const std::string& owner,
			const std::string& target,
			Side side,
			Type type,
			double price,
			long quantity
		) : cl_ord_id(clOrdId)
		  , symbol(symbol)
		  , owner(owner)
		  , target(target)
		  , side(side)
		  , type(type)
		  , price(price)
		  , quantity(quantity)
		{
			open_quantity = quantity;
			executed_quantity = 0;
			avg_executed_price = 0;
			last_executed_price = 0;
			last_executed_quantity = 0;
		}

		const std::string& Order::get_client_id() const { return cl_ord_id; }

		const std::string& Order::get_symbol() const { return symbol; }

		const std::string& Order::get_owner() const { return owner; }

		const std::string& Order::get_target() const { return target; }

		Order::Side Order::get_side() const { return side; }

		Order::Type Order::getType() const { return type; }

		double Order::get_price() const { return price; }

		long Order::get_quantity() const { return quantity; }

		long Order::get_open_quantity() const { return open_quantity; }

		long Order::get_executed_quantity() const { return executed_quantity; }

		double Order::get_avg_executed_price() const { return avg_executed_price; }

		double Order::get_last_executed_price() const { return last_executed_price; }

		long Order::get_last_executed_quantity() const { return last_executed_quantity; }

		bool Order::isFilled() const { return quantity == executed_quantity; }

		bool Order::isClosed() const { return open_quantity == 0; }

		void Order::execute(double price, long quantity)
		{
			avg_executed_price =
				((quantity * price) + (avg_executed_price * executed_quantity))
				/ (quantity + executed_quantity);

			open_quantity -= quantity;
			executed_quantity += quantity;
			last_executed_price = price;
			last_executed_quantity = quantity;
		}

		void Order::cancel()
		{
			open_quantity = 0;
		}

		std::string Order::to_string() const {
			return
				"symbol=" + symbol + ", " +
				"clOrdId=" + cl_ord_id + ", " +
				"owner=" + owner + ", " +
				"target=" + target + ", " +
				"side=" + std::to_string(side) + ", " +
				"type=" + std::to_string(type) + ", " +
				"price=" + std::to_string(price) + ", " +
				"quantity=" + std::to_string(quantity) + ", " +
				"openQuantity=" + std::to_string(open_quantity) + ", " +
				"executedQuantity=" + std::to_string(executed_quantity) + ", " +
				"avgExecutedPrice=" + std::to_string(avg_executed_price) + ", " +
				"lastExecutedPrice=" + std::to_string(last_executed_price) + ", " +
				"lastExecutedQuantity=" + std::to_string(last_executed_quantity);
		}


	std::ostream& operator<<(std::ostream& ostream, const Order& order)
	{
		return ostream << order.to_string();
	}
}

