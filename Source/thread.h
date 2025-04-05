#ifndef DC_THREAD_H
#define DC_THREAD_H
#include <cstddef>
#include <cstdint>

#define DC_SH4 1
namespace dc {

class Thread {
public:
	using RunFunction = void *(*)(void *);

	Thread() = default;

	Thread(const char *label,
	    size_t stackSize,
	    bool detached,
	    RunFunction runFunction,
	    void *param = nullptr);

	~Thread();

	bool spawn(const char *label,
	    size_t stackSize,
	    bool detached,
	    RunFunction runFunction,
	    void *param = nullptr);

	bool join();

	bool isValid() const { return !!nativeHandle_; }
	bool isJoinable() const { return !detached_; }

private:
	uintptr_t nativeHandle_ = 0;
	bool detached_ = false;
};

} // namespace dc

#endif
