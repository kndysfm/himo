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
		virtual void OnDraw(void) = 0;
		virtual void Invalidate(void) = 0;
	};

	template<typename KeyType>
	class BoundBase : public IBinder<KeyType>
	{
	protected:
		bool invalidated_;
		IBinder<KeyType> *binder_;

		BoundBase() : binder_(nullptr), invalidated_(true) { }

		virtual void Invalidate(void) sealed
		{
			invalidated_ = true;
			if (binder_) binder_->Invalidate();
		}
	};

	template<typename KeyType, typename ValueType>
	class BoundData: public BoundBase<KeyType>
	{
	private:
		std::recursive_mutex mtx_v_, mtx_k_;
		std::vector<KeyType> bound_keys_;
		std::function<ValueType (KeyType)> func_getter_;
		std::function<void (KeyType, ValueType)> func_setter_;
		std::function<int (ValueType, ValueType)> func_comparator_;
		std::function<ValueType (ValueType)> func_validator_;
		ValueType value_;

		void update(ValueType v)
		{
			std::lock_guard<std::recursive_mutex> lock(mtx_v_);
			value_ = v;
			Invalidate();
		}

		virtual void OnCommand(KeyType key) sealed
		{
			if (!func_getter_) return;
			std::lock_guard<std::recursive_mutex> lock(mtx_v_);

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
				std::lock_guard<std::recursive_mutex> lock(mtx_k_);

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

		virtual void OnDraw(void) sealed
		{
			if (invalidated_)
			{
				invalidated_ = false;
				if (!func_setter_) return;
				std::lock_guard<std::recursive_mutex> lock_v(mtx_v_);

				if (func_validator_ && func_comparator_)
				{
					ValueType v_validated = func_validator_(value_);
					if (func_comparator_(value_, v_validated) != 0)
					{	// invalid value
						return; // not update
					}
				}

				std::lock_guard<std::recursive_mutex> lock_k(mtx_k_);
				for (auto k : bound_keys_)
				{
					func_setter_(k, value_);
				}
			}
		}

		virtual bool Bind(IBinder *binder, KeyType key) sealed
		{
			binder_ = binder;

			std::lock_guard<std::recursive_mutex> lock_k(mtx_k_);
			bound_keys_.push_back(key);
			std::lock_guard<std::recursive_mutex> lock_v(mtx_v_);
			if (func_setter_) func_setter_(key, value_);
			return TRUE;
		}
	public:

		BoundData(ValueType iniValue): value_(iniValue){ }

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
	class BoundCommand : public BoundBase<KeyType>
	{
	private:
		std::vector<KeyType> bound_keys_;
		bool enabled_;
		std::function<void(KeyType, bool)> func_enabler_;
		bool async_;
		std::thread thread_;
		std::recursive_mutex mtx_en_, mtx_k_;
		std::function<void(KeyType)> func_action_;

		void update(bool executable)
		{
			std::lock_guard<std::recursive_mutex> lock(mtx_en_);
			enabled_ = executable;
			Invalidate();
		}

		void action_async(KeyType key)
		{
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

		virtual void OnDraw(void) sealed
		{
			if (invalidated_)
			{
				invalidated_ = false;
				if (!func_enabler_) return;

				std::lock_guard<std::recursive_mutex> lock_en(mtx_en_);
				std::lock_guard<std::recursive_mutex> lock_k(mtx_k_);
				for (auto k : bound_keys_)
				{
					func_enabler_(k, enabled_);
				}
			}
		}

		virtual bool Bind(IBinder *binder, KeyType key) sealed
		{
			binder_ = binder;

			std::lock_guard<std::recursive_mutex> lock_k(mtx_k_);
			bound_keys_.push_back(key);
			std::lock_guard<std::recursive_mutex> lock_en(mtx_en_);
			if (func_enabler_) func_enabler_(key, enabled_);
			return TRUE;
		}

	public:

		BoundCommand(bool executable = true) :enabled_(executable), async_(false) { }
		virtual ~BoundCommand() { if (thread_.joinable()) thread_.join(); }

		void Enable(bool executable) { update(executable); }
		bool IsEnabled() const { return enabled_; }

		void AttachEnabler(std::function<void(KeyType, bool)> enabler) { func_enabler_ = enabler; }
		void AttachAction(std::function<void(KeyType)> action, bool async = false) { func_action_ = action; async_ = async; }
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
		std::function<void(void)> func_invalidator_;
		bool invalidated_;

	public:
		virtual bool Bind(IBinder *bound, KeyType key) sealed
		{
			if (!bound->Bind(this, key)) return FALSE;

			bindings_[key].push_back(bound);
			return TRUE;
		}

		virtual void OnCommand(KeyType key) sealed
		{
			if (bindings_.count(key) == 1)
			{
				for (auto b : bindings_[key])
				{
					b->OnCommand(key); 
				}
			}
		}

		virtual void OnNotify(KeyType key) sealed
		{
			if (bindings_.count(key) == 1)
			{
				for (auto b : bindings_[key])
				{
					b->OnNotify(key);
				} 
			}
		}

		virtual void OnDraw(void) sealed
		{
			if (invalidated_)
			{
				invalidated_ = false;
				for (auto pair : bindings_)
				{
					for (auto b : pair.second) b->OnDraw();
				}
			}
		}

		virtual void Invalidate(void) sealed
		{
			if (!invalidated_)
			{
				invalidated_ = true;
				if (func_invalidator_) func_invalidator_();
			}
		}

		void AttachInvalidator(std::function<void(void)> invalidator) { func_invalidator_ = invalidator; }

		Binder() : invalidated_(true) { }

	};
}
