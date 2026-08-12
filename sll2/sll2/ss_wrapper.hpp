#pragma once
#include <forward_list>
#include <typeinfo>
#include <typeindex>
#include <variant>
#include <vector>
#include <tuple>
#include <array>
#include <stdexcept>

using Value = std::variant<std::monostate,long long,double,bool>;

class ClassImp {
public:
	ClassImp() = default;
	virtual ~ClassImp() = default;
	virtual void* Build() = 0;
	virtual void Delete(void* obj) = 0;
	virtual size_t Size() = 0;
	virtual const std::type_info& TypeInfo() = 0;
	virtual std::type_index TypeIndex() = 0;
};


template<typename Class>
class ClassBaseWrapper : public ClassImp {
public:
	
	virtual void* Build() override {
		return new Class();
	}
	virtual void Delete(void* obj) override {
		Class* o = static_cast<Class*>(obj);
		delete o;
	}
	virtual size_t Size() override {
		return sizeof(Class);
	}
	virtual const std::type_info& TypeInfo() override {
		return typeid(Class);
	}
	virtual std::type_index TypeIndex() override {
		return typeid(Class);
	}
};
class ClassWrapper {
private:
	ClassImp* cw = nullptr;
	ClassWrapper() {}
public:
	template<typename Class>
	static ClassWrapper* Create() {
		auto ret = new ClassWrapper();
		ret->cw = new ClassBaseWrapper<Class>();
		return ret;
	};
	ClassImp* GetClass() {
		return cw;
	}
	~ClassWrapper() {
		delete cw;
	}
};

template<typename T, typename... Args>
inline constexpr bool is_one_of_v = (... || std::is_same_v<T, Args>);

template<typename T>
static inline T safe_cast(const Value& v) {
	return std::visit([](auto&& arg) -> T {
		using Src = std::decay_t<decltype(arg)>;
		using Dest = std::decay_t<T>;
		if constexpr ((is_one_of_v<Src, long long,double,bool>
			&& is_one_of_v<Dest,double,float,int,char,short,size_t,unsigned,bool>)) {
			return arg;
		}
		return  T();
		}, v);
}

template<typename T>
static inline Value cast_value(const T& v) {
	using Src = std::decay_t<T>;
	if constexpr (is_one_of_v<Src,long long, size_t,long,int,unsigned,short,char>) {
		return Value((long long)v);
	}else if constexpr (is_one_of_v<Src, double, float, long double>) {
		return Value((double)v);
	}else if constexpr (std::is_same_v<Src,bool>) {
		return Value(v);
	}
	return Value();
}

class FunctionImp {
public:
	virtual Value call(void* object, const Value* args) const = 0;
	virtual size_t argcount() const = 0;
	virtual const std::type_info& argtype(size_t) const = 0;
};

template<typename Class, typename Ret, typename... Args>
class FuncWrapper : public FunctionImp {
private:
	using Func = Ret(Class::*)(Args ...);
	using ClassTuple = std::tuple<Args...>; //参数类型元组
	static constexpr std::size_t num = sizeof...(Args);
	Func func = nullptr;
	template<std::size_t...I>
	Value temp_call(Class* obj, const Value* args, std::index_sequence<I...>) const {
		if constexpr (std::is_same_v<Ret, void>) {
			(obj->*func)(safe_cast<Args>(args[I])...);
			return Value();
		}else return cast_value((obj->*func)(safe_cast<Args>(args[I])...));
	}
public:
	FuncWrapper(Func func) : func(func) {}
	virtual size_t argcount() const override { return num; }
	virtual Value call(void* object, const Value* args) const override {
		auto obj = static_cast<Class*>(object);
		if (obj) {
			return temp_call(obj, args, std::index_sequence_for<Args ...>{});
		}else throw std::bad_cast();
		return Value();
	};
	virtual const std::type_info& argtype(size_t n) const override {
		static const auto type_arr = []<std::size_t ...I>(std::index_sequence<I ...>){
			return std::array<const std::type_info*,num>{
				&typeid(std::tuple_element_t<I, ClassTuple>) ...
			};
		}(std::index_sequence_for<Args ...>{});
		if (n >= num) throw std::out_of_range("argtype index out of range");
		return *type_arr[n];
	};
};
