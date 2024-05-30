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
  std::cerr << "semantic error: " << error_msg << '\n';
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

Features getFeatures(tree_node *node) {
  GetFeatures visitor;
  node->accept(visitor);
  return visitor.features;
}

std::string getName(tree_node *node) {
  GetName visitor;
  node->accept(visitor);
  return std::string(visitor.name);
}

std::string getParentName(tree_node *node) {
  GetParent visitor;
  node->accept(visitor);
  return std::string(visitor.name);
}

std::string getType(tree_node *node) {
  GetType visitor;
  node->accept(visitor);
  return std::string(visitor.type);
}

Formals getFormals(tree_node *node) {
  GetFormals visitor;
  node->accept(visitor);
  return visitor.formals;
}

Expression getExpression(tree_node *node) {
  GetExpression visitor;
  node->accept(visitor);
  return visitor.expr;
}

Expressions getExpressions(tree_node *node) {
  GetExpressions visitor;
  node->accept(visitor);
  return visitor.exprs;
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
    // semantic::dump_symtables(idtable, stringtable, inttable);

    std::unordered_map<std::string, std::string> classes_hierarchy;
    std::unordered_map<std::string, std::string> var_to_type;
    std::unordered_set<std::string> non_inherited{"Bool", "Int", "String",
                                                  "SELF_TYPE"};
    std::unordered_set<std::string> classes_names(non_inherited);
    classes_names.insert("Object");

    // Loop through classes
    for (int i = parse_results->first(); parse_results->more(i);
         i = parse_results->next(i)) {

      class__class *current_class = dynamic_cast<class__class *>(parse_results->nth(i));
      std::string class_name = semantic::getName(current_class);

      // Check unique class name
      auto result = classes_names.insert(class_name);
      if (!result.second) {
        semantic::error("class '" + std::string(class_name) +
                        "' already exists!");
      }

      // Add class to inheritance hierarchy
      std::string parent_name = semantic::getParentName(current_class);
      classes_hierarchy[class_name] = parent_name;

      // Check that parent class isn't builtin (except 'Object')
      if (non_inherited.find(parent_name) != non_inherited.end()) {
        semantic::error("class '" + class_name + "': can't use parent class '" +
                        parent_name + "' (builtin)");
      }

      Features features = semantic::getFeatures(current_class);
      std::unordered_set<std::string> features_names;

      // Loop through features
      for (int j = features->first(); features->more(j);
           j = features->next(j)) {

        Feature current_feature = features->nth(j);
        std::string feature_name = semantic::getName(current_feature);

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
        std::string feature_type = semantic::getType(current_feature);

        // Type existence check
        if (classes_names.find(feature_type) == classes_names.end()) {
          semantic::error("unknown type '" + feature_type + "' in " + feature_name);
        }

        // SELF_TYPE check
        if (feature_type == "SELF_TYPE") {
          semantic::error("can't use SELF_TYPE as a type inside class");
        }

        if (current_feature->get_feature_type() == "method_class") {
          Formals formals = semantic::getFormals(current_feature);

          // Check method overrides - must have same signature
          if (std::string(parent_name) != "Object") {
            class__class *parent =
                semantic::FindClass(parent_name, parse_results);

            if (parent) {
              Features parent_features = semantic::getFeatures(parent);

              // Loop through parent features
              for (int a = parent_features->first(); parent_features->more(a);
                   a = parent_features->next(a)) {
                Feature parent_feature = parent_features->nth(a);
                std::string parent_feature_name = semantic::getName(parent_feature);

                // If there is parent feature with same name
                if (parent_feature_name == feature_name) {

                  // Check if feature is same type
                  if (parent_feature->get_feature_type() !=
                      current_feature->get_feature_type()) {
                    semantic::error("wrong override of feature '" +
                                    feature_name + "' from class '" +
                                    parent_name + "' in class '" + class_name +
                                    "'");
                  }

                  // Check method signatures
                  method_class *cur_method =
                      dynamic_cast<method_class *>(current_feature);
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
            } else { // If parent doesn't exist
              semantic::error("parent class '" + parent_name +
                              "' of class '" + class_name + "' doesn't exist");
            }
          }

          // Local formals names
          std::unordered_set<std::string> formals_names;

          // Loop through formals
          for (int k = formals->first(); formals->more(k);
               k = formals->next(k)) {
            Formal_class *current_formal = dynamic_cast<formal_class *>(formals->nth(k));
            std::string formal_name = semantic::getName(current_formal);

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

            std::string formal_type = semantic::getType(current_formal);
            // Check formal type
            if (classes_names.find(formal_type) == classes_names.end()) {
              semantic::error("unknown type '" + formal_type + "' in " + formal_name);
            }

            // Get method expression
            Expression expr = semantic::getExpression(current_feature);

            // block_class check
            if (expr->get_expr_type() == "block_class") {
              // Get expressions from block
              Expressions exprs = semantic::getExpressions(expr);

              // Block expressions check
              for (int l = exprs->first(); exprs->more(l); l = exprs->next(l)) {
                Expression current_expr = exprs->nth(l);

                // let
                if (current_expr->get_expr_type() == "let_class") {
                  formal_name = semantic::getName(current_expr);

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
                  std::string expr_type = semantic::getType(current_expr);
                  if (classes_names.find(expr_type) == classes_names.end()) {
                    semantic::error("unknown type '" + expr_type + "' in " +
                                    formal_name);
                  }
                }
              }
            }
          }
        } else { // attr_class
          // Check init expression
          attr_class* attr = dynamic_cast<attr_class*>(current_feature);
          semantic::check_builtin_types_init(semantic::getType(attr), semantic::getExpression(attr));
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
