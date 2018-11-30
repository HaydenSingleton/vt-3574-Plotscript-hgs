#include "catch.hpp"

#include <utility>
#include "TSmessage.hpp"
#include "expression.hpp"

TEST_CASE( "Test TSmessage default constructor", "[TSmessage]" ) {

    typedef std::pair<std::string, Expression> Item;
    TSmessage<Item> myqueue;

    REQUIRE(myqueue.empty());
}

TEST_CASE( "Test TSmessage push", "[TSmessage]" ) {

    typedef std::pair<std::string, Expression> Item;
    TSmessage<Item> myqueue;

    Item a = {"rawr x3", Expression(Atom("uWu"))};
    myqueue.push(a);

    REQUIRE_FALSE(myqueue.empty());
}

TEST_CASE( "Test TSmessage trypop", "[TSmessage]" ) {

    typedef std::pair<std::string, Expression> Item;
    TSmessage<Item> myqueue;

    Item a = {"rawr x3", Expression(Atom("uWu"))};
    REQUIRE_FALSE(myqueue.try_pop(a));
    myqueue.push(a);
    REQUIRE(myqueue.try_pop(a));
    REQUIRE(myqueue.empty());
}

TEST_CASE( "Test TSmessage wait and pop", "[TSmessage]" ) {

    typedef std::pair<std::string, Expression> Item;
    TSmessage<Item> myqueue;

    Item a = {"rawr x3", Expression(Atom("uWu"))}, b;
    REQUIRE(a!=b);
    myqueue.push(a);
    myqueue.wait_and_pop(b);
    REQUIRE(myqueue.empty());
    REQUIRE(a==b);
}