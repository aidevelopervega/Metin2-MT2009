#ifndef __INC_LIBTHECORE_EVENT_H__
#define __INC_LIBTHECORE_EVENT_H__

#include <cstddef>
#include <utility>

#ifdef M2_USE_POOL
#include "pool.h"
#endif

/**
 * Base class for all event info data
 */
struct event_info_data
{
	event_info_data() {}
	virtual ~event_info_data() {}

#ifdef M2_USE_POOL
	static void* operator new(size_t size) {
		return pool_.Acquire(size);
	}
	static void operator delete(void* p, size_t size) {
		pool_.Release(p, size);
	}
private:
	static MemoryPool pool_;
#endif
};

typedef struct event EVENT;

extern void intrusive_ptr_add_ref(EVENT* p);
extern void intrusive_ptr_release(EVENT* p);

template <class T>
class intrusive_ptr
{
public:
	constexpr intrusive_ptr() noexcept : px_(nullptr) {}

	intrusive_ptr(T* p, bool add_ref = true) : px_(p)
	{
		if (px_ != nullptr && add_ref)
			intrusive_ptr_add_ref(px_);
	}

	intrusive_ptr(const intrusive_ptr& rhs) : px_(rhs.px_)
	{
		if (px_ != nullptr)
			intrusive_ptr_add_ref(px_);
	}

	intrusive_ptr(intrusive_ptr&& rhs) noexcept : px_(rhs.px_)
	{
		rhs.px_ = nullptr;
	}

	~intrusive_ptr()
	{
		if (px_ != nullptr)
			intrusive_ptr_release(px_);
	}

	intrusive_ptr& operator=(const intrusive_ptr& rhs)
	{
		intrusive_ptr(rhs).swap(*this);
		return *this;
	}

	intrusive_ptr& operator=(intrusive_ptr&& rhs) noexcept
	{
		intrusive_ptr(std::move(rhs)).swap(*this);
		return *this;
	}

	intrusive_ptr& operator=(T* rhs)
	{
		intrusive_ptr(rhs).swap(*this);
		return *this;
	}

	void reset() noexcept
	{
		intrusive_ptr().swap(*this);
	}

	void reset(T* rhs)
	{
		intrusive_ptr(rhs).swap(*this);
	}

	T* get() const noexcept { return px_; }
	T& operator*() const noexcept { return *px_; }
	T* operator->() const noexcept { return px_; }
	explicit operator bool() const noexcept { return px_ != nullptr; }
	bool operator!() const noexcept { return px_ == nullptr; }

	void swap(intrusive_ptr& rhs) noexcept
	{
		std::swap(px_, rhs.px_);
	}

	friend T* get_pointer(const intrusive_ptr& p) noexcept
	{
		return p.px_;
	}

	friend bool operator==(const intrusive_ptr& a, const intrusive_ptr& b) noexcept { return a.px_ == b.px_; }
	friend bool operator!=(const intrusive_ptr& a, const intrusive_ptr& b) noexcept { return a.px_ != b.px_; }
	friend bool operator==(const intrusive_ptr& a, const T* b) noexcept { return a.px_ == b; }
	friend bool operator!=(const intrusive_ptr& a, const T* b) noexcept { return a.px_ != b; }
	friend bool operator==(const T* a, const intrusive_ptr& b) noexcept { return a == b.px_; }
	friend bool operator!=(const T* a, const intrusive_ptr& b) noexcept { return a != b.px_; }
	friend bool operator<(const intrusive_ptr& a, const intrusive_ptr& b) noexcept { return a.px_ < b.px_; }

private:
	T* px_;
};

typedef intrusive_ptr<EVENT> LPEVENT;
typedef long (*TEVENTFUNC) (LPEVENT event, long processing_time);

#define EVENTFUNC(name)	long (name) ([[maybe_unused]] LPEVENT event, [[maybe_unused]] long processing_time)
#define EVENTINFO(name) struct name : public event_info_data

struct TQueueElement;

struct event
{
	event() : func(NULL), info(NULL), q_el(NULL), ref_count(0) {}
	~event() {
		if (info != NULL) {
#ifdef M2_USE_POOL
			delete info;
#else
			M2_DELETE(info);
#endif
		}
	}
	TEVENTFUNC			func;
	event_info_data* 	info;
	TQueueElement *		q_el;
	char				is_force_to_end;
	char				is_processing;

	size_t ref_count;
};

template<class T> // T should be a subclass of event_info_data
T* AllocEventInfo() {
#ifdef M2_USE_POOL
	return new T;
#else
	return M2_NEW T;
#endif
}

extern void		event_destroy();
extern int		event_process(int pulse);
extern int		event_count();

#define event_create(func, info, when) event_create_ex(func, info, when)
extern LPEVENT	event_create_ex(TEVENTFUNC func, event_info_data* info, long when);
extern void		event_cancel(LPEVENT * event);
extern long		event_processing_time(LPEVENT event);
extern long		event_time(LPEVENT event);
extern void		event_reset_time(LPEVENT event, long when);
extern void		event_set_verbose(int level);

extern event_info_data* FindEventInfo(DWORD dwID);
extern event_info_data*	event_info(LPEVENT event);

#define EVENT_CANCEL_EX(event) if (event) { event_cancel(&event); event = nullptr; }
#endif

