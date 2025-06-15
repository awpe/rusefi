#include "pch.h"
#include "type_list.h"

struct A{};
struct B{};
struct C{};
struct D{};

TEST(type_list, init) {
	type_list<A,B,C,D> list;

	static_assert(list.count() == 4);
	static_assert(decltype(list)::count<A>() == 1);
	static_assert(decltype(list)::count<B>() == 1);
	static_assert(decltype(list)::count<C>() == 1);
	static_assert(decltype(list)::count<D>() == 1);

	ASSERT_EQ(1, 1);
}
