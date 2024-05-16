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
    if (hierarchy.find(entry.second) == hierarchy.end() &&
        entry.second != "Object") {
      error("parent of class '" + entry.first + "' ('" + entry.second +
            "') doesn't exist");
    }
    if (dfs(entry.first)) {
      return true; // Loop detected
    }
  }
  return false; // No loop detected
}

bool CheckSignatures(method_class *m1, method_class *m2) {
  // Get return types
  GetType m1_type_visitor;
  GetType m2_type_visitor;
  m1->accept(m1_type_visitor);
  m2->accept(m2_type_visitor);
  // Check methods return types
  if (m1_type_visitor.type != m2_type_visitor.type) {
    return false;
  }

  // Get formals
  GetFormals m1_formals_visitor;
  GetFormals m2_formals_visitor;
  m1->accept(m1_formals_visitor);
  m2->accept(m2_formals_visitor);
  Formals m1_formals = m1_formals_visitor.formals;
  Formals m2_formals = m2_formals_visitor.formals;

  // Check formals count
  if (m1_formals->len() != m2_formals->len()) {
    return false;
  }

  // Loop through formals
  for (int i = m1_formals->first(); m1_formals->more(i);
       i = m1_formals->next(i)) {
    formal_class *m1_formal = dynamic_cast<formal_class *>(m1_formals->nth(i));
    formal_class *m2_formal = dynamic_cast<formal_class *>(m2_formals->nth(i));

    // Get formal names
    GetName m1_formal_name_visitor;
    GetName m2_formal_name_visitor;
    m1_formal->accept(m1_formal_name_visitor);
    m2_formal->accept(m2_formal_name_visitor);
    std::string name1 = m1_formal_name_visitor.name;
    std::string name2 = m2_formal_name_visitor.name;
    // Check formal names
    if (name1 != name2) {
      return false;
    }

    // Get formal types
    GetType m1_formal_type_visitor;
    GetType m2_formal_type_visitor;
    m1_formal->accept(m1_formal_type_visitor);
    m2_formal->accept(m2_formal_type_visitor);
    std::string type1 = m1_formal_type_visitor.type;
    std::string type2 = m2_formal_type_visitor.type;
    // Check formal types
    if (type1 != type2) {
      return false;
    }
  }
  // All good - signatures are the same
  return true;
}

class__class *FindClass(std::string name, Classes classes) {
  for (int i = classes->first(); classes->more(i); i = classes->next(i)) {
    GetName name_visitor;
    class__class *cur_class = dynamic_cast<class__class *>(classes->nth(i));
    cur_class->accept(name_visitor);
    if (name == name_visitor.name) {
      return cur_class;
    }
  }
  return nullptr;
}

void dump_symtables(IdTable idtable, StrTable strtable, IntTable inttable) {
  ast_root->dump_with_types(std::cerr, 0);
  std::cerr << "# Identifiers:\n";
  idtable.print();
  std::cerr << "# Strings:\n";
  stringtable.print();
  std::cerr << "# Integers:\n";
  inttable.print();
}

void check_builtin_types_init(std::string type, Expression expr) {
  std::string expr_type = expr->get_expr_type();
  if (expr_type == "no_expr_class") {
    return;
  }
  if (type == "Int" && expr_type != "int_const_class") {
    error("initialization of Int with non-integer value");
  } else if (type == "Bool" && expr_type != "bool_const_class") {
    error("initialization of Bool with non-boolean value");
  } else if (type == "String" && expr_type != "string_const_class") {
    error("initialization of String with non-string value");
  }
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
    // semantic::dump_symtables(idtable, stringtable, inttable);

    std::unordered_set<std::string> non_inherited{"Bool", "Int", "String",
                                                  "SELF_TYPE"};
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

      // Check unique class name
      auto result = classes_names.insert(class_name);
      if (!result.second) {
        semantic::error("class '" + std::string(class_name) +
                        "' already exists!");
      }

      // Add class to inheritance hierarchy
      GetParent parent_visitor;
      parse_results->nth(i)->accept(parent_visitor);
      std::string parent_name = parent_visitor.name;
      classes_hierarchy[class_name] = parent_name;

      // Check that parent class isn't builtin (except 'Object')
      if (non_inherited.find(parent_name) != non_inherited.end()) {
        semantic::error("class '" + class_name + "': can't use parent class '" +
                        parent_name + "' (builtin)");
      }

      // Get class features
      GetFeatures features_visitor;
      parse_results->nth(i)->accept(features_visitor);
      Features features = features_visitor.features;
      std::unordered_set<std::string> features_names;

      // Loop through features
      for (int j = features->first(); features->more(j);
           j = features->next(j)) {

        // Current feature
        Feature feature = features->nth(j);

        // Get feature name
        feature->accept(name_visitor);
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
        feature->accept(type_visitor);

        // Type existence check
        std::string type = type_visitor.type;
        if (classes_names.find(type) == classes_names.end()) {
          semantic::error("unknown type '" + type + "' in " + feature_name);
        }

        // SELF_TYPE check
        if (type == "SELF_TYPE") {
          semantic::error("can't use SELF_TYPE as a type inside class");
        }

        if (feature->get_feature_type() == "method_class") {
          // Methods formals
          GetFormals formals_visitor;
          feature->accept(formals_visitor);
          Formals formals = formals_visitor.formals;

          // Check method overrides - must have same signature
          if (std::string(parent_name) != "Object") {
            // Get parent class features
            GetFeatures parent_features_visitor;
            class__class *parent =
                semantic::FindClass(parent_name, parse_results);
            if (parent) {
              parent->accept(parent_features_visitor);
              Features parent_features = parent_features_visitor.features;

              // Loop through parent features
              for (int a = parent_features->first(); parent_features->more(a);
                   a = parent_features->next(a)) {
                Feature parent_feature = parent_features->nth(a);

                // Get feature name
                parent_feature->accept(name_visitor);
                std::string parent_feature_name = name_visitor.name;

                // If there is parent feature with same name
                if (parent_feature_name == feature_name) {

                  // Check if feature is same type
                  if (parent_feature->get_feature_type() !=
                      feature->get_feature_type()) {
                    semantic::error("wrong override of feature '" +
                                    feature_name + "' from class '" +
                                    parent_name + "' in class '" + class_name +
                                    "'");
                  }

                  // Check method signatures
                  method_class *cur_method =
                      dynamic_cast<method_class *>(feature);
                  method_class *parent_method =
                      dynamic_cast<method_class *>(parent_feature);
                  if (!semantic::CheckSignatures(cur_method, parent_method)) {
                    semantic::error(
                        "'" + feature_name + "' method from class '" +
                        parent_name +
                        "' doesn't match override version of it in class '" +
                        class_name + "'");
                  }
                }
              }
            } else {
              semantic::error("parent class '" + std::string(parent_name) +
                              "' of class '" + class_name + "' doesn't exist");
            }
          }

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

                // let
                if (current->get_expr_type() == "let_class") {

                  // Get let-expr variable name
                  current->accept(name_visitor);
                  formal_name = name_visitor.name;

                  // 'self' name check
                  if (formal_name == "self") {
                    semantic::error(
                        "can't use 'self' as new local variable name");
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
        } else { // attr_class
          // Check init expression
          attr_class* attr = dynamic_cast<attr_class*>(feature);
          attr->accept(type_visitor);
          GetExpression getExpr;
          attr->accept(getExpr);
          semantic::check_builtin_types_init(type_visitor.type, getExpr.expr);
        }
      }
      // Check existence of method main in class Main
      if (class_name == "Main" &&
          features_names.find("main") == features_names.end()) {
        semantic::error("No method 'main' in class 'Main'");
      }

      // Dump all features
      // semantic::sequence_out("Features (methods + attributes) of '" + class_name + '\'', features_names);
    }

    // Check existence of class Main
    if (classes_names.find("Main") == classes_names.end()) {
      semantic::error("class Main doesn't exist");
    }

    // Dump all classes
    // semantic::sequence_out("Classes (types)", classes_names);

    // Inheritance hierarchy loop check
    if (semantic::detect_cycle(classes_hierarchy)) {
      semantic::error("loop detected in classes inheritance hierarchy");
      std::cerr << "\\ program classes' hierarchy (child : parent)\n";
      for (auto p : classes_hierarchy) {
        std::cerr << '\t' << p.first << " : " << p.second << "\n";
      }
    }
    std::fclose(token_file);
  }
  std::cerr << "# Detected " << semantic::err_count << " semantic errors\n";
  return semantic::err_count;
}
