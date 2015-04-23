// Ricardo Marmolejo Garcia
// 15-04-2015
// experimental reinterpretation pattern command
#include <iostream>
#include <tuple>
#include <functional>
#include <vector>
#include <string>
#include <sstream>
#include <queue>
#include <fast-event-system/fes.h>
#include <future>
#include <chrono>
#include <assert.h>

namespace lead {

template <typename T> using CommandTalker = std::function<void(T&)>;
template <typename T> using CompositeCommandTalker = std::function<CommandTalker<T>(const CommandTalker<T>&)>;

template <typename T>
CommandTalker<T> operator>>=(const CompositeCommandTalker<T>& a, const CommandTalker<T>& b)
{
	return a(b);
}

template <typename T>
CompositeCommandTalker<T> repeat(int n)
{
	return [=](const CommandTalker<T>& f)
	{
		return [&](T& self)
		{
			// I want coroutines :(
			for(int cont = 0; cont < n; ++cont)
			{
				f(self);
			}
		};
	};
}

template <typename T>
class talker
{
public:
	using container = fes::queue_delayer<CommandTalker<T> >;
	
	talker(const std::string& name)
		: _name(name)
		, _thread(nullptr)
		, _idle(true)
	{
		
	}
	~talker()
	{
		
	}
	
	void add_follower(T& talker)
	{
		_conns.emplace_back(_queue.connect(std::bind(&talker::planificator, *this, std::ref(talker), std::placeholders::_1)));
	}
	
	void planificator(T& talker, const CommandTalker<T>& cmd)
	{
		std::cout << "new work" << std::endl;
		assert(_idle == true);
		
		//std::packaged_task<int(T&)> pt([&](T& parm) -> int {cmd(parm); return 0; });
		std::packaged_task<void(T&)> pt(cmd);
		
		_future = pt.get_future();
		_thread = std::make_shared<std::thread>(std::move(pt), std::ref(talker));
		_thread->detach();
	}
	
	inline void order(const CommandTalker<T>& command, int milli = 0, int priority = 0)
	{
		_queue(priority, std::chrono::milliseconds(milli), command);
	}
	
	void update()
	{
		if (_idle)
		{
			if(_queue.dispatch())
			{
				_idle = false;
			}
		}
		else
		{
			if (_future.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready)
			{
				//_thread->join();
				_thread = nullptr;
				_idle = true;
			}
		}
	}

	inline bool empty() const
	{
		return _queue.empty();
	}
	
	inline void wait(int milli)
	{
		std::this_thread::sleep_for( std::chrono::milliseconds(milli) );
	}
protected:
	std::vector<fes::shared_connection<CommandTalker<T> > > _conns;
	container _queue;
	std::string _name;
	std::shared_ptr<std::thread> _thread;
	std::shared_future<void> _future;
	bool _idle;
};

}

class Context
{
public:
	explicit Context()
	{
		
	}
	
	~Context() { ; }
	
	// skills
	void print(const std::string& text)
	{
		_lock.lock();

		for(const char& c : text)
		{
			std::cout << c << std::flush;
			//std::this_thread::sleep_for( std::chrono::milliseconds(100) );
		}
		std::cout << std::endl;

		_lock.unlock();
	}
protected:
	std::mutex _lock;
};

class Buyer;

// client side
// ShopKeeper can be a lider of liders
class ShopKeeper : public lead::talker<Buyer>
{
public:
	explicit ShopKeeper(const std::string& name, Context& context)
		: talker(name)
		, _context(context)
	{
		
	}
	
	~ShopKeeper() { ; }
	
	// skills
	void say(const std::string& text)
	{
		_context.print(text);
	}
protected:
	Context& _context;
};


class Buyer : public lead::talker< ShopKeeper >
{
public:
	explicit Buyer(const std::string& name, Context& context)
		: talker(name)
		, _context(context)
	{
		
	}
	~Buyer() { ; }	

	// skills
	void say(const std::string& text)
	{
		_context.print(text);
	}
protected:
	Context& _context;
};

int main()
{
	{
		Context context;
		auto buyer = Buyer("buyer", context);
		auto shopkeeper = ShopKeeper("shopkeeper", context);
		
		buyer.add_follower(shopkeeper);
		shopkeeper.add_follower(buyer);
		
		// conversation between tyrants
		
		buyer.order([&](ShopKeeper& self) {
				self.say("Hi!");
		}, 1);

		shopkeeper.order([&](Buyer& self) {
				self.say("How much cost are the apples?");
		}, 2);

		buyer.order([&](ShopKeeper& self) {
				self.say("50 cents each apple"); 
		}, 3);

		shopkeeper.order([&](Buyer& self) {
				self.say("I want apples!");
		}, 4);

		buyer.order([&](ShopKeeper& self) {
				self.say("You apples, thanks"); 
		}, 5);

		shopkeeper.order([&](Buyer& self) {
				self.say("Bye!");
		}, 6);
	
		for(int i=0; i<9000;++i)	
		{
			buyer.update();
			shopkeeper.update();

			std::this_thread::sleep_for( std::chrono::milliseconds(1) );
		}
	}
	return(0);
}

