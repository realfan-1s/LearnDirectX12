#pragma once

#include <memory>

template <typename T>
class Singleton {
public:
	static T& instance();
	Singleton(const Singleton&) = delete;
	Singleton(Singleton&&) = delete;
	Singleton& operator=(const Singleton&) = delete;
	Singleton& operator=(Singleton&&) = delete;
protected:
	struct Token {};
	Singleton() = default;
	virtual ~Singleton() = default;
};

template <typename T>
T& Singleton<T>::instance()
{
	static std::unique_ptr<T> instance(new T(Token{}));
	return *instance;
}