class Main {
  main() : Array {
    42
  };
};

class B inherits A {
  a : Heap;
  c : Int <- "wow";
  d : String <- 1;
  e : Bool <- 2;

  some_method(h : Hashtab): Int {
    {
      let h: Float <- 4 in false;
      let self: Int <- 5 in true;
      10;
    }
  };
};

class C {
  a : Int <- 5;
  b : String <- "wow";
  c : Bool <- true;

  const_int_plus_bool(): Int {
    5 + true
  };

  var_int_plus_string(): Int {
    a + b
  };

  var_int_plus_bool(): Int {
    a + c
  };

  int_plus_int(): Int {
    5 + 10
  };
};