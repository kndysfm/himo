#pragma once

#include <functional>
#include <vector>
#include <map>
#include <thread>
#include <mutex>

namespace himo
{
	template<typename KeyType>
	class IBinder
	{
	private:
		IBinder(IBinder const &) = delete;
		IBinder(IBinder &&) = delete;
		IBinder & operator=(IBinder const &) = delete;
		IBinder & operator=(IBinder &&) = delete;

	public:
		IBinder() = default;
		virtual bool Bind(IBinder<KeyType> *, KeyType) = 0;
		virtual void OnCommand(KeyType) = 0;
		virtual void OnNotify(KeyType) = 0;
	};

	template<typename KeyType, typename ValueType>
	class BoundData: public IBinder<KeyType>
	{
	private:
		std::vector<KeyType> bound_keys_;
		std::function<ValueType (KeyType)> func_getter_;
		std::function<void (KeyType, ValueType)> func_setter_;
		std::function<int (ValueType, ValueType)> func_comparator_;
		std::function<ValueType (ValueType)> func_validator_;
		ValueType value_;

		void update(ValueType v)
		{
			value_ = v;
			if (!func_setter_) return;

			if (func_validator_ && func_comparator_)
			{
				ValueType v_validated = func_validator_(v);
				if (func_comparator_(v, v_validated) != 0)
				{	// invalid value
					return; // not update
				}
			}

			for (auto k : bound_keys_)
			{
				func_setter_(k, v);
			}
		}

		virtual void OnCommand(KeyType key) sealed
		{
			if (!func_getter_) return;

			ValueType v = func_getter_(key);
			
			if (func_validator_ && func_comparator_)
			{
				ValueType v_validated = func_validator_(v);
				if (func_comparator_(v, v_validated) != 0)
				{	// invalid value
					func_setter_(key, value_);
					return; // not update
				}
			}

			if (!func_comparator_ || func_comparator_(value_, v) != 0)
			{
				value_ = v;
				if (!func_setter_) return;

				for (auto k : bound_keys_)
				{
					if (k != key) func_setter_(k, v);
				}
			}
		}

		virtual void OnNotify(KeyType key) sealed
		{
			OnCommand(key);
		}

		virtual bool Bind(IBinder *binder, KeyType key) sealed
		{
			if (!::IsWindow(key)) return FALSE;

			bound_keys_.push_back(key);
			if (func_setter_) func_setter_(key, value_);
			return TRUE;
		}
	public:

		BoundData(ValueType iniValue):value_(iniValue) { }

		void SetValue(ValueType v) { update(v); }
		ValueType GetValue() const { return value_; }
		operator ValueType() const { return value_; }
		BoundData & operator= (const ValueType& v) { update(v); return *this; }
		void AttachGetter(std::function<ValueType(KeyType)> getter) { func_getter_ = getter; }
		void AttachSetter(std::function<void(KeyType, ValueType)> setter) { func_setter_ = setter; }
		void AttachComparator(std::function<int(ValueType, ValueType)> comparator) { func_comparator_ = comparator; }
		void AttachValidator(std::function<ValueType(ValueType)> validator) { func_validator_ = validator; }
		void CopyBehavior(BoundData const &src)
		{
			func_getter_ = src.func_getter_;
			func_setter_ = src.func_setter_;
			func_comparator_ = src.func_comparator_;
			func_validator_ = src.func_validator_;
		}
	};

	template<typename KeyType>
	class BoundCommand : public IBinder<KeyType>
	{
	private:
		std::vector<KeyType> bound_keys_;
		bool enabled_;
		std::function<void(KeyType, bool)> func_enabler_;
		bool async_;
		std::thread thread_;
		std::mutex mtx_;
		std::function<void(KeyType)> func_action_;

		void update(bool executable)
		{
			enabled_ = executable;
			if (!func_enabler_) return;

			for (auto k : bound_keys_)
			{
				func_enabler_(k, executable);
			}
		}

		void action_async(KeyType key)
		{
			std::lock_guard<std::mutex> lock(mtx_);
			update(false);
			func_action_(key);
			update(true);
		}

		virtual void OnCommand(KeyType key) sealed
		{
			if (!enabled_) return;

			if (thread_.joinable()) thread_.join();

			if (async_)
			{
				std::thread th(std::bind(&BoundCommand::action_async, this, key));
				if (th.joinable())
				{
					thread_.swap(th);
				}
			}
			else
			{
				func_action_(key);
			}
		}

		virtual void OnNotify(KeyType key) sealed
		{
			//OnCommand(key);
		}

		virtual bool Bind(IBinder *binder, KeyType key) sealed
		{
			if (!::IsWindow(key)) return FALSE;

			bound_keys_.push_back(key);
			if (func_enabler_) func_enabler_(key, enabled_);
			return TRUE;
		}

	public:

		BoundCommand(bool executable = true) :enabled_(executable), async_(false) { }
		virtual ~BoundCommand() { if (thread_.joinable()) thread_.join(); }

		void Enable(bool executable) { std::lock_guard<std::mutex> lock(mtx_); update(executable); }
		bool IsEnabled() const { return enabled_; }

		void AttachEnabler(std::function<void(KeyType, bool)> enabler) { std::lock_guard<std::mutex> lock(mtx_); func_enabler_ = enabler; }
		void AttachAction(std::function<void(KeyType)> action, bool async = false) { std::lock_guard<std::mutex> lock(mtx_); func_action_ = action; async_ = async; }
		void CopyBehavior(BoundCommand const &src)
		{
			func_enabler_ = src->func_enabler_;
			func_action_ = src->func_action_;
		}
	};

	template<typename KeyType>
	class Binder: public IBinder<KeyType>
	{
	private:
		std::map<KeyType, std::vector<IBinder<KeyType> *>> bindings_;

	public:
		virtual bool Bind(IBinder *bound, KeyType key) sealed
		{
			if (!::IsWindow(key)) return FALSE;

			if (!bound->Bind(this, key)) return FALSE;

			bindings_[key].push_back(bound);
			return TRUE;
		}

		virtual void OnCommand(KeyType key) sealed
		{
			if (bindings_.count(key) == 1)
			{
				for (auto b: bindings_[key]) b->OnCommand(key);
			}
		}

		virtual void OnNotify(KeyType key) sealed
		{
			if (bindings_.count(key) == 1)
			{
				for (auto b : bindings_[key]) b->OnNotify(key);
			}
		}
	};
}
