#pragma once
#include <cstdint>
struct TextView
{
	const char* _Data;
	// [1-end_of_zero][...otherbit...]
	size_t      _Length;

	template<typename T>
	TextView(T& stdContainer) {
		_Data = (const char*)stdContainer.data();
		constexpr auto elementSize = sizeof(stdContainer[0]);
		_Length = stdContainer.size() * elementSize;
		_Length = ~((~_Length) >> 1);
	}

	TextView(const char* s) {
		_Data = s;
		_Length = strlen(s);
		_Length = ~((~_Length) >> 1);
	}

	TextView(const char* s, size_t n) {
		_Data = s;
		_Length = ~((~n) >> 1);
	}

	// 取长度
	inline size_t GetLength() const {
		size_t mask = ((size_t)1 << 31) - 1;
		return _Length & mask;
	}

	// 判空
	inline bool IsEmpty() const {
		return _Length == 0;
	}

	// 是否为0结尾
	inline bool IsEndOfEmpty() const {
		constexpr size_t mask = ~((~(size_t)0) >> 1);
		return (_Length & mask) != 0;
	}

	// 获取指针
	inline const char* GetData() const {
		return _Data;
	}

	// 下标访问
	inline char operator[](size_t i) const {
		return _Data[i];
	}
};

