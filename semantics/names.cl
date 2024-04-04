-- wrong

class A {
    a : Int;
};

class A { -- class repeat
    a : Int;
};

class B {
    a : Int;
    a : Int; -- field repeat

    foo(num: Int, num: Int): Int { -- params repeat
        5;
    };

    bar(num: Int): Int {
        5;
    };

    bar(num: Int): Int { -- method repeat
        5;
    };

    foobar(num: Int): Int {
        5;
    };

    foofoobar(num: Int): Int {
        let a: Int <- 4 in false; -- class field override
        {
            let a: Int <- 5 in true; -- local variable override
        };
        {
            let b: Int <- 6 in false;
        };
        b - 3; -- use out of scope
    };
};


class C inherits B {
    a : Int; -- field override

    foobar(num: Int): Int { -- method override
        5;
    };
};
