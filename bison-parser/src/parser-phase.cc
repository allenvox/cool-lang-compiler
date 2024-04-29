#include "cool-parse.h"
#include "cool-tree.h"
#include "utilities.h"
#include <cstdio>
#include <functional>
#include <iostream>
#include <unistd.h>
#include <unordered_map>
#include <unordered_set>

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

void sequence_out(std::string title, std::unordered_set<std::string> set) {
  std::cerr << title << ": ";
  for (auto s : set) {
    std::cerr << s << ' ';
  }
  std::cerr << '\n';
}

bool detect_cycle(std::unordered_map<std::string, std::string> hierarchy) {
  std::unordered_set<std::string> visited;
  std::unordered_set<std::string> currentlyVisiting;

  std::function<bool(const std::string &)> dfs =
      [&](const std::string &className) {
        // If already visited, no need to visit again
        if (visited.find(className) != visited.end()) {
          return false;
        }
        // If currently visiting, loop detected
        if (currentlyVisiting.find(className) != currentlyVisiting.end()) {
          return true;
        }
        // Mark as currently visiting
        currentlyVisiting.insert(className);
        // Get parent class
        auto it = hierarchy.find(className);
        if (it != hierarchy.end()) {
          // Recursive DFS call for parent class
          if (dfs(it->second)) {
            return true;
          }
        }
        // Mark as visited and remove from currently visiting
        visited.insert(className);
        currentlyVisiting.erase(className);
        return false;
      };

  // Iterate over each class in the hierarchy
  for (const auto &entry : hierarchy) {
    if (dfs(entry.first)) {
      return true; // Loop detected
    }
  }
  return false; // No loop detected
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

    /* ???

      Class_ Object_class = class_(
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
    );
     */

    /*
    // Symtables dumps
    ast_root->dump_with_types(std::cerr, 0);
    std::cerr << "# Identifiers:\n";
    idtable.print();
    std::cerr << "# Strings:\n";
    stringtable.print();
    std::cerr << "# Integers:\n";
    inttable.print();
     */

    std::unordered_set<std::string> classes_names{"Object", "Bool", "Int",
                                                  "String", "SELF_TYPE"};
    std::unordered_map<std::string, std::string> classes_hierarchy;

    GetName name_visitor;
    for (int i = parse_results->first(); parse_results->more(i);
         i = parse_results->next(i)) {
      // Classes' unique non-std names check
      parse_results->nth(i)->accept(name_visitor);
      std::string class_name = name_visitor.name;
      if (class_name == "SELF_TYPE") {
        semantic::error("SELF_TYPE redeclared!");
      }
      auto result = classes_names.insert(class_name);
      if (!result.second) {
        semantic::error("class '" + std::string(class_name) +
                        "' already exists!");
      }

      // Add class to inheritance hierarchy
      GetParent parent_visitor;
      parse_results->nth(i)->accept(parent_visitor);
      classes_hierarchy[class_name] = std::string(parent_visitor.parent);

      // Get features
      GetFeatures features_visitor;
      parse_results->nth(i)->accept(features_visitor);
      Features features = features_visitor.features;

      // Loop through features
      std::unordered_set<std::string> features_names;
      for (int j = features->first(); features->more(j);
           j = features->next(j)) {
        // Features' unique names check
        features->nth(j)->accept(name_visitor);
        std::string feature_name = name_visitor.name;
        result = features_names.insert(feature_name);
        if (!result.second) {
          semantic::error("feature '" + std::string(feature_name) + "' in '" +
                          class_name + "' already exists!");
        }

        // Features' types existence check
        GetType type_visitor; // methods - return_type, attrs - type_decl
        features->nth(j)->accept(type_visitor);
        std::string type = type_visitor.type;
        if (classes_names.find(type) == classes_names.end()) {
          semantic::error("unknown type '" + type + "' in " + feature_name);
        }

        // Methods' formals
        GetFormals formals_visitor;
        features->nth(j)->accept(formals_visitor);
        Formals formals = formals_visitor.formals;

        if (formals_visitor.formals != nullptr) {        // method_class check
          std::unordered_set<std::string> formals_names; // Local formals' names

          for (int k = formals->first(); formals->more(k);
               k = formals->next(k)) {
            // Formal unique name check
            formals->nth(k)->accept(name_visitor);
            std::string formal_name = name_visitor.name;
            result = formals_names.insert(formal_name);
            if (!result.second) {
              semantic::error("formal '" + std::string(formal_name) + "' in '" +
                              feature_name + "' already exists!");
            }

            // Local formal type check
            formals->nth(k)->accept(type_visitor);
            type = type_visitor.type;
            if (classes_names.find(type) == classes_names.end()) {
              semantic::error("unknown type '" + type + "' in " + formal_name);
            }

            // Methods' expression
            GetExpression expr_visitor;
            features->nth(j)->accept(expr_visitor);
            Expression expr =
                expr_visitor.expr; // only for block_class since let-formals
                                   // aren't used without block

            // Get Expressions from nested block
            GetExpressions exprs_visitor;
            expr->accept(exprs_visitor);
            Expressions exprs = exprs_visitor.exprs;

            // Nested let-variables check
            for (int l = exprs->first(); exprs->more(l); l = exprs->next(l)) {
              // Let-expr formal name check
              if (let_class *current =
                      dynamic_cast<let_class *>(exprs->nth(l))) {
                current->accept(name_visitor);
                formal_name = name_visitor.name;
                result = formals_names.insert(formal_name);

                if (!result.second) {
                  semantic::error("formal '" + std::string(formal_name) +
                                  "' in '" + feature_name + "' from '" +
                                  class_name + "' already exists!");
                }

                // Let-expr formal type check
                current->accept(type_visitor);
                type = type_visitor.type;
                if (classes_names.find(type) == classes_names.end()) {
                  semantic::error("unknown type '" + type + "' in " +
                                  formal_name);
                }
              }
            }
          }
        }
      }
      semantic::sequence_out("Features (methods + attributes) of '" +
                                 class_name + '\'',
                             features_names);
    }
    semantic::sequence_out("Classes (types)", classes_names);

    // Inheritance hierarchy loop check
    if (semantic::detect_cycle(classes_hierarchy)) {
      semantic::error("loop detected in classes inheritance hierarchy");
    }

    std::fclose(token_file);
  }
  return 0;
}
