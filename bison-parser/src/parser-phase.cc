#include "cool-parse.h"
#include "cool-tree.h"
#include "utilities.h"
#include <cstdio>
#include <iostream>
#include <unistd.h>
#include <set>
#include <typeinfo>

std::FILE *token_file = stdin;
extern Classes parse_results;
extern Program ast_root;

extern int curr_lineno;
const char *curr_filename = "<stdin>";
extern int parse_errors;

// Debug flags
extern int yy_flex_debug;
extern int cool_yydebug;
int lex_verbose = 0;

extern int cool_yyparse();

namespace semantic {

void error(std::string error_msg) {
  std::cerr << "\tsemantic error: " << error_msg << '\n';
}

void sequence_out(std::string title, std::set<std::string> set) {
  std::cerr << title << ": ";
  for(auto s : set) {
    std::cerr << s << ' ';
  }
  std::cerr << '\n';
}

}; // namespace semantic

int main(int argc, char **argv) {
  yy_flex_debug = 0;
  cool_yydebug = 0;
  lex_verbose = 0;

  for (int i = 1; i < argc; i++) {
    token_file = std::fopen(argv[i], "r");
    if (token_file == NULL) {
      std::cerr << "Error: can not open file " << argv[i] << std::endl;
      std::exit(1);
    }
    curr_lineno = 1;

    cool_yyparse();
    if (parse_errors != 0) {
      std::cerr << "Error: parse errors\n";
      std::exit(1);
    }

    // Добавление в AST класса (узла) для встроенных типов
    Symbol filename = stringtable.add_string("<builtin-classes>");

    Symbol Object = idtable.add_string("Object");
    Symbol Bool = idtable.add_string("Bool");
    Symbol Int = idtable.add_string("Int");
    Symbol String = idtable.add_string("String");
    Symbol SELF_TYPE = idtable.add_string("SELF_TYPE");

    /*Class_ Object_class = class_(
      Object,
      nil_Classes(),
      append_Features(
        append_Features(
          single_Features(method(cool_abort, nil_Formals(), Object, no_expr())),
          single_Features(method(type_name, nil_Formals(), Str, no_expr()))
        ),
        single_Features(method(copy, nil_Formals(), SELF_TYPE, no_expr()))
      ),
      filename
    );*/

    /* TODO
     * 1. Предварительно добавить в таблицу символов такие имена как:
     *    Main, main, Object, SELF_TYPE, ..., self, String, Int, Bool
     * 2. Создать вектор с именами всех классов
     * 3. Добавить метод GetParent в класс class__class
     * 4. Добавить в список классов (AST) builtin-классы: Int, String, ...
     * 5. Создать таблицу методов, для каждого класса
     */

    /*
    ast_root->dump_with_types(std::cerr, 0);
    std::cerr << "# Identifiers:\n";
    idtable.print();
    std::cerr << "# Strings:\n";
    stringtable.print();
    std::cerr << "# Integers:\n";
    inttable.print();
     */

    std::set<std::string> classes_names{"Object", "Bool", "Int", "String", "SELF_TYPE"}, features_names;
    GetName name_visitor;
    for (int i = parse_results->first(); parse_results->more(i); i = parse_results->next(i)) {
      parse_results->nth(i)->accept(name_visitor);
      std::string class_name = name_visitor.name;
      if (class_name == "SELF_TYPE") {
        semantic::error("SELF_TYPE redeclared!");
      }
      auto result = classes_names.insert(class_name);
      if(!result.second) {
        semantic::error("class '" + std::string(class_name) + "' already exists!");
      }

      GetFeatures features_visitor;
      parse_results->nth(i)->accept(features_visitor);
      Features features = features_visitor.features;

      for (int j = features->first(); features->more(i); i = features->next(i)) {
        features->nth(i)->accept(name_visitor);
        std::string feature_name = name_visitor.name;
        result = features_names.insert(feature_name);
        if(!result.second) {
          semantic::error("feature '" + std::string(feature_name) + "' in '" + class_name + "' already exists!");
        }

        GetType type_visitor;
        features->nth(i)->accept(type_visitor);
        std::string type = type_visitor.type;
        if(classes_names.find(type) == classes_names.end()) {
          semantic::error("unknown type '" + type + "' in " + feature_name);
        }
      }
      semantic::sequence_out("Features of '" + class_name + '\'', features_names);
    }
    semantic::sequence_out("Classes", classes_names);

    std::fclose(token_file);
  }
  return 0;
}
