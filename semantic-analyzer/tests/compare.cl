class Main {
  main(): Int {
    42
  };

  a : Int <- 5;
  b : String <- "wow";
  c : Bool <- true;

  var_int_int(): Bool {
    a < 8
  };

  neg_bool(): Bool {
    ~true
  };

  neg_string(): Bool {
    ~"Hi"
  };

  neg_int(): Bool {
    ~42
  };

  var_int_string(): Bool {
    a < b
  };

  eq_int_string(): Bool {
    5 = "wow"
  };
};
