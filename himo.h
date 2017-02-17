#pragma once

#include <windows.h>

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
		std::vector<KeyType> bound_ctrls_;
		std::function<ValueType (KeyType)> func_reader_;
		std::function<void (KeyType, ValueType)> func_writer_;
		std::function<int (ValueType, ValueType)> func_comparator_;
		ValueType value_;

		void update(ValueType v, KeyType root = NULL)
		{
			value_ = v;
			if (!func_writer_) return;

			for (auto c : bound_ctrls_)
			{
				if (c != root) func_writer_(c, v);
			}
		}

		virtual void OnCommand(KeyType key) sealed
		{
			if (!func_reader_) return;

			ValueType v = func_reader_(key);
			if (!func_comparator_ || func_comparator_(value_, v) != 0)
			{
				update(v, key);
			}
		}

		virtual void OnNotify(KeyType key) sealed
		{
			OnCommand(key);
		}

		virtual bool Bind(IBinder *binder, KeyType key) sealed
		{
			if (!::IsWindow(key)) return FALSE;

			bound_ctrls_.push_back(key);
			if (func_writer_) func_writer_(key, value_);
			return TRUE;
		}
	public:

		BoundData(ValueType iniValue):value_(iniValue) { }

		void SetValue(ValueType v) { update(v); }
		ValueType GetValue() const { return value_; }
		operator ValueType() const { return value_; }
		BoundData & operator= (const ValueType& v) { update(v); return *this; }
		void AttachReader(std::function<ValueType(KeyType)> reader) { func_reader_ = reader; }
		void AttachWriter(std::function<void(KeyType, ValueType)> writer) { func_writer_ = writer; }
		void AttachComparator(std::function<int(ValueType, ValueType)> comparator) { func_comparator_ = comparator; }
	};

	template<typename KeyType>
	class BoundCommand : public IBinder<KeyType>
	{
	private:
		std::vector<KeyType> bound_ctrls_;
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

			for (auto c : bound_ctrls_)
			{
				func_enabler_(c, executable);
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

			bound_ctrls_.push_back(key);
			if (func_enabler_) func_enabler_(key, enabled_);
			return TRUE;
		}

	public:

		BoundCommand(bool executable = true) :enabled_(executable), async_(false) { }

		void Enable(bool executable) { std::lock_guard<std::mutex> lock(mtx_); update(executable); }
		bool IsEnabled() const { return enabled_; }

		void AttachEnabler(std::function<void(KeyType, bool)> enabler) { std::lock_guard<std::mutex> lock(mtx_); func_enabler_ = enabler; }
		void AttachAction(std::function<void(KeyType)> action, bool async = false) { std::lock_guard<std::mutex> lock(mtx_); func_action_ = action; async_ = async; }

	};

	template<typename KeyType>
	class Binder: public IBinder<KeyType>
	{
	private:
		std::map<KeyType, std::vector<IBinder<KeyType> *>> bounds_;

	public:
		virtual bool Bind(IBinder *bound, KeyType key) sealed
		{
			if (!::IsWindow(key)) return FALSE;

			if (!bound->Bind(this, key)) return FALSE;

			bounds_[key].push_back(bound);
			return TRUE;
		}

		virtual void OnCommand(KeyType key) sealed
		{
			if (bounds_.count(key) == 1)
			{
				for (auto b: bounds_[key]) b->OnCommand(key);
			}
		}

		virtual void OnNotify(KeyType key) sealed
		{
			if (bounds_.count(key) == 1)
			{
				for (auto b : bounds_[key]) b->OnNotify(key);
			}
		}
	};
}
