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

int err_count = 0;
void error(std::string error_msg) {
  std::cerr << "# semantic error: " << error_msg << '\n';
  err_count++;
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

    // Check parent existence
    if (hierarchy.find(entry.second)) {
      error("Parent of class '" + entry.first + "' ('" + entry.second + "') doesn't exist\n");
      err_count++;
    }

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

    /*
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

    std::unordered_set<std::string> std_classes{"Object", "Bool", "Int", "String", "SELF_TYPE"};
    std::unordered_set<std::string> classes_names{"Object", "Bool", "Int",
                                                  "String", "SELF_TYPE"};
    std::unordered_map<std::string, std::string> classes_hierarchy;
    GetName name_visitor;

    // Loop through classes
    for (int i = parse_results->first(); parse_results->more(i);
         i = parse_results->next(i)) {

      // Get current class name
      parse_results->nth(i)->accept(name_visitor);
      std::string class_name = name_visitor.name;

      // Check non-std class name
      if (std_classes.find(class_name) != std_classes.end()) {
        semantic::error("standard type " + class_name + " redeclared!");
      }

      // Check unique class name
      auto result = classes_names.insert(class_name);
      if (!result.second) {
        semantic::error("class '" + std::string(class_name) +
                        "' already exists!");
      }

      // Add class to inheritance hierarchy
      GetParent parent_visitor;
      parse_results->nth(i)->accept(parent_visitor);
      classes_hierarchy[class_name] = std::string(parent_visitor.parent);

      // TODO
      // check method overrides - parent method must have same signature
      // check initializers

      // Get class features
      GetFeatures features_visitor;
      parse_results->nth(i)->accept(features_visitor);
      Features features = features_visitor.features;

      std::unordered_set<std::string> features_names;

      // Loop through features
      for (int j = features->first(); features->more(j);
           j = features->next(j)) {

        // Get feature name
        features->nth(j)->accept(name_visitor);
        std::string feature_name = name_visitor.name;

        // 'self' name check
        if (feature_name == "self") {
          semantic::error("can't use 'self' as feature name");
        }

        // Check unique feature name
        result = features_names.insert(feature_name);
        if (!result.second) {
          semantic::error("feature '" + std::string(feature_name) + "' in '" +
                          class_name + "' already exists!");
        }

        // Get feature type: methods - return_type, attrs - type_decl
        GetType type_visitor;
        features->nth(j)->accept(type_visitor);

        // Type existence check
        std::string type = type_visitor.type;
        if (classes_names.find(type) == classes_names.end()) {
          semantic::error("unknown type '" + type + "' in " + feature_name);
        }

        // SELF_TYPE check
        if (type == "SELF_TYPE") {
          semantic::error("can't use SELF_TYPE as a type inside class");
        }

        // Methods formals
        GetFormals formals_visitor;
        features->nth(j)->accept(formals_visitor);
        Formals formals = formals_visitor.formals;

        // method_class check
        if (formals_visitor.formals != nullptr) {

          // Local formals names
          std::unordered_set<std::string> formals_names;

          // Loop through formals
          for (int k = formals->first(); formals->more(k);
               k = formals->next(k)) {

            // Get formal name
            formals->nth(k)->accept(name_visitor);
            std::string formal_name = name_visitor.name;

            // 'self' name check
            if (formal_name == "self") {
              semantic::error("can't use 'self' as formal name");
            }

            // Unique name check
            result = formals_names.insert(formal_name);
            if (!result.second) {
              semantic::error("formal '" + std::string(formal_name) + "' in '" +
                              feature_name + "' already exists!");
            }

            // Get formal type
            formals->nth(k)->accept(type_visitor);
            type = type_visitor.type;

            // Check formal type
            if (classes_names.find(type) == classes_names.end()) {
              semantic::error("unknown type '" + type + "' in " + formal_name);
            }

            // Get method expression
            GetExpression expr_visitor;
            features->nth(j)->accept(expr_visitor);
            Expression expr = expr_visitor.expr;

            // block_class check
            if (expr->get_expr_type() == "block_class") {

              // Get expressions from block
              GetExpressions exprs_visitor;
              expr->accept(exprs_visitor);
              Expressions exprs = exprs_visitor.exprs;

              // Block expressions check
              for (int l = exprs->first(); exprs->more(l); l = exprs->next(l)) {
                Expression current = exprs->nth(l);

                // Let_class
                if (current->get_expr_type() == "let_class") {

                  // Get let-expr variable name
                  current->accept(name_visitor);
                  formal_name = name_visitor.name;

                  // 'self' name check
                  if (formal_name == "self") {
                    semantic::error("can't use 'self' as new local variable name");
                  }

                  // Check unique of nested formal
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
      }

      // Check existence of method main in class Main
      if (class_name == "Main" && features_names.find("main") == features_names.end()) {
        semantic::error("No method 'main' in class 'Main'");
      }

      // Dump all features
      semantic::sequence_out("Features (methods + attributes) of '" +
                                 class_name + '\'',
                             features_names);
    }

    // Check existence of class Main
    if (classes_names.find("Main") == classes_names.end()) {
      semantic::error("class Main doesn't exist");
    }

    // Dump all classes
    semantic::sequence_out("Classes (types)", classes_names);

    // Inheritance hierarchy loop check
    if (semantic::detect_cycle(classes_hierarchy)) {
      semantic::error("loop detected in classes inheritance hierarchy");
    }

    std::fclose(token_file);
  }

  std::cerr << "# Detected " << semantic::err_count << " semantic errors\n";
  return semantic::err_count;
}
