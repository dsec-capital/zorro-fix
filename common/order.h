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
		) : clOrdId(clOrdId),
			symbol(symbol),
			owner(owner),
			target(target),
			side(side),
			type(type),
			price(price),
			quantity(quantity)
		{
			openQuantity = quantity;
			executedQuantity = 0;
			avgExecutedPrice = 0;
			lastExecutedPrice = 0;
			lastExecutedQuantity = 0;
		}

		const std::string& getClientID() const { return clOrdId; }
		const std::string& getSymbol() const { return symbol; }
		const std::string& getOwner() const { return owner; }
		const std::string& getTarget() const { return target; }
		Side getSide() const { return side; }
		Type getType() const { return type; }
		double getPrice() const { return price; }
		long getQuantity() const { return quantity; }

		long getOpenQuantity() const { return openQuantity; }
		long getExecutedQuantity() const { return executedQuantity; }
		double getAvgExecutedPrice() const { return avgExecutedPrice; }
		double getLastExecutedPrice() const { return lastExecutedPrice; }
		long getLastExecutedQuantity() const { return lastExecutedQuantity; }

		bool isFilled() const { return quantity == executedQuantity; }
		bool isClosed() const { return openQuantity == 0; }

		void execute(double price, long quantity)
		{
			avgExecutedPrice =
				((quantity * price) + (avgExecutedPrice * executedQuantity))
				/ (quantity + executedQuantity);

			openQuantity -= quantity;
			executedQuantity += quantity;
			lastExecutedPrice = price;
			lastExecutedQuantity = quantity;
		}

		void cancel()
		{
			openQuantity = 0;
		}

		std::string toString() const {
			return 
				"symbol=" + symbol + ", " +
				"clOrdId=" + clOrdId + ", " +
				"owner=" + owner + ", " +
				"target=" + target + ", " +
				"side=" + std::to_string(side) + ", " +
				"type=" + std::to_string(type) + ", " +
				"price=" + std::to_string(price) + ", " +
				"quantity=" + std::to_string(quantity) + ", " +
				"openQuantity=" + std::to_string(openQuantity) + ", " +
				"executedQuantity=" + std::to_string(executedQuantity) + ", " +
				"avgExecutedPrice=" + std::to_string(avgExecutedPrice) + ", " +
				"lastExecutedPrice=" + std::to_string(lastExecutedPrice) + ", " +
				"lastExecutedQuantity=" + std::to_string(lastExecutedQuantity);
		}

	private:
		std::string clOrdId;
		std::string symbol;
		std::string owner;
		std::string target;
		Side side;
		Type type;
		double price;
		long quantity;
		long openQuantity;
		long executedQuantity;
		double avgExecutedPrice;
		double lastExecutedPrice;
		long lastExecutedQuantity;
	};

	inline std::ostream& operator<<(std::ostream& ostream, const Order& order)
	{
		return ostream << order.toString();
	}
}

#endif
